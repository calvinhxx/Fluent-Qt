#include "components/status_info/Avatar.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTextBoundaryFinder>
#include <QVariant>

#include "compatibility/QtCompat.h"
#include "components/status_info/InfoBadge.h"
#include "design/Typography.h"

namespace fluent::status_info {
namespace {

QColor contrastingTextColor(const QColor& background)
{
    const qreal luminance =
        0.2126 * background.redF()
        + 0.7152 * background.greenF()
        + 0.0722 * background.blueF();
    return luminance > 0.56 ? QColor(20, 20, 20) : QColor(Qt::white);
}

QString leftGraphemes(const QString& text, int count)
{
    if (text.isEmpty() || count <= 0)
        return QString();

    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    finder.toStart();
    int end = 0;
    for (int index = 0; index < count; ++index) {
        const int next = finder.toNextBoundary();
        if (next < 0)
            break;
        end = next;
        if (end >= text.size())
            break;
    }
    return text.left(end);
}

} // namespace

Avatar::Avatar(QWidget* parent)
    : QWidget(parent)
    , m_presenceBadge(new InfoBadge(this))
{
    setObjectName(QStringLiteral("fluentAvatar"));
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_presenceBadge->setObjectName(QStringLiteral("fluentAvatarPresenceBadge"));
    m_presenceBadge->setDisplayMode(
        InfoBadge::InfoBadgeDisplayMode::Dot);
    m_presenceBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_presenceBadge->hide();

    updateFixedExtent();
    updatePresenceBadge();
}

Avatar::Avatar(const QString& name, QWidget* parent)
    : Avatar(parent)
{
    setName(name);
}

void Avatar::setName(const QString& name)
{
    if (m_name == name)
        return;

    const QString previousName = m_name;
    const bool tracksName = accessibleName().isEmpty()
        || accessibleName() == previousName;
    m_name = name;
    if (tracksName)
        setAccessibleName(m_name);
    update();
    emit nameChanged(m_name);
}

void Avatar::setInitials(const QString& initials)
{
    const QString normalized = leftGraphemes(initials.trimmed(), 2);
    if (m_initials == normalized)
        return;

    m_initials = normalized;
    update();
    emit initialsChanged(m_initials);
}

void Avatar::setImage(const QPixmap& image)
{
    if (m_image.cacheKey() == image.cacheKey()
        && qFuzzyCompare(m_image.devicePixelRatioF(),
                         image.devicePixelRatioF())) {
        return;
    }

    m_image = image;
    update();
    emit imageChanged(m_image);
}

void Avatar::setShape(AvatarShape shape)
{
    if (m_shape == shape)
        return;

    m_shape = shape;
    update();
    emit shapeChanged(m_shape);
}

void Avatar::setAvatarSize(AvatarSize size)
{
    if (m_avatarSize == size)
        return;

    m_avatarSize = size;
    updateFixedExtent();
    updatePresenceBadge();
    updateGeometry();
    update();
    emit avatarSizeChanged(m_avatarSize);
}

void Avatar::setPresence(PresenceStatus presence)
{
    if (m_presence == presence)
        return;

    m_presence = presence;
    updatePresenceBadge();
    update();
    emit presenceChanged(m_presence);
}

void Avatar::setBackgroundColor(const QColor& color)
{
    if (m_backgroundColor == color)
        return;

    m_backgroundColor = color;
    update();
    emit backgroundColorChanged(m_backgroundColor);
}

void Avatar::setForegroundColor(const QColor& color)
{
    if (m_foregroundColor == color)
        return;

    m_foregroundColor = color;
    update();
    emit foregroundColorChanged(m_foregroundColor);
}

QString Avatar::effectiveInitials() const
{
    if (!m_initials.isEmpty())
        return m_initials;

    const QString normalizedName = m_name.simplified();
    if (normalizedName.isEmpty())
        return QString();

    const QStringList words =
        normalizedName.split(QRegularExpression(QStringLiteral("\\s+")),
                             Qt::SkipEmptyParts);
    if (words.isEmpty())
        return QString();
    if (words.size() == 1)
        return leftGraphemes(words.first(), 2).toUpper();

    const QString first = leftGraphemes(words.first(), 1);
    const QString last = leftGraphemes(words.last(), 1);
    return (layoutDirection() == Qt::RightToLeft
                ? last + first
                : first + last)
        .toUpper();
}

QSize Avatar::sizeHint() const
{
    const int extent = avatarExtent();
    return QSize(extent, extent);
}

QSize Avatar::minimumSizeHint() const
{
    return sizeHint();
}

void Avatar::onThemeUpdated()
{
    updatePresenceBadge();
    update();
}

void Avatar::paintEvent(QPaintEvent*)
{
    if (width() <= 0 || height() <= 0)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF avatarRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clipPath;
    if (m_shape == AvatarShape::Circular) {
        clipPath.addEllipse(avatarRect);
    } else {
        const qreal radius = themeRadius().control;
        clipPath.addRoundedRect(avatarRect, radius, radius);
    }

    const QColor background = effectiveBackgroundColor();
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawPath(clipPath);

    if (!m_image.isNull()) {
        painter.save();
        painter.setClipPath(clipPath);
        if (!isEnabled())
            painter.setOpacity(0.55);
        // Use the shared cover helper so Qt 5/6 HiDPI source rectangles stay
        // correct. zh_CN: 走共享 cover 辅助，保证 Qt 5/6 HiDPI 源矩形正确。
        fluentDrawCoverPixmapInLogicalRect(painter, avatarRect, m_image);
        painter.restore();
    } else {
        const QString initialsText = effectiveInitials();
        if (!initialsText.isEmpty()) {
            Typography::FontRole role = Typography::FontRole::Caption;
            if (avatarExtent() >= 48)
                role = Typography::FontRole::BodyLargeStrong;
            else if (avatarExtent() >= 32)
                role = Typography::FontRole::BodyStrong;
            painter.setFont(themeFont(role).toQFont());
            painter.setPen(effectiveForegroundColor(background));
            painter.drawText(avatarRect, Qt::AlignCenter, initialsText);
        } else {
            painter.setPen(effectiveForegroundColor(background));
            Typography::Icons::paintGlyph(
                painter,
                avatarRect,
                Typography::Icons::Contact,
                qMax(12, avatarExtent() / 2),
                Qt::AlignCenter);
        }
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(themeColorsRef().strokeCard, 1.0));
    painter.drawPath(clipPath);

    if (m_presence != PresenceStatus::None && m_presenceBadge->isVisible()) {
        const QRect badgeGeometry = m_presenceBadge->geometry();
        painter.setPen(Qt::NoPen);
        painter.setBrush(surroundingSurfaceColor());
        painter.drawEllipse(QRectF(badgeGeometry).adjusted(
            0.5, 0.5, -0.5, -0.5));
    }
}

void Avatar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePresenceBadge();
}

void Avatar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange
        || event->type() == QEvent::LayoutDirectionChange) {
        updatePresenceBadge();
        update();
    }
}

int Avatar::avatarExtent() const
{
    return static_cast<int>(m_avatarSize);
}

QColor Avatar::effectiveBackgroundColor() const
{
    const auto& colors = themeColorsRef();
    QColor color = m_backgroundColor;
    if (!color.isValid()) {
        color = colors.accentDefault;
        if (!m_name.isEmpty() && !colors.charts.isEmpty())
            color = colors.charts.at(
                qHash(m_name, 0u) % colors.charts.size());
    }
    if (!color.isValid())
        color = colors.controlSecondary;
    if (!isEnabled())
        color.setAlpha(120);
    return color;
}

QColor Avatar::effectiveForegroundColor(const QColor& background) const
{
    if (!isEnabled())
        return themeColorsRef().textDisabled;
    if (m_foregroundColor.isValid())
        return m_foregroundColor;
    return contrastingTextColor(background);
}

QColor Avatar::surroundingSurfaceColor() const
{
    for (QWidget* ancestor = parentWidget();
         ancestor;
         ancestor = ancestor->parentWidget()) {
        const QVariant surface = ancestor->property("fluentSurfaceColor");
        if (surface.canConvert<QColor>()) {
            const QColor color = surface.value<QColor>();
            if (color.isValid())
                return color;
        }
    }
    return themeColorsRef().bgCanvas;
}

void Avatar::updatePresenceBadge()
{
    if (!m_presenceBadge)
        return;

    const bool visible = m_presence != PresenceStatus::None;
    m_presenceBadge->setVisible(visible);
    if (!visible)
        return;

    const int dotDiameter = avatarExtent() <= 24
        ? 6
        : (avatarExtent() <= 40 ? 8 : 10);
    const int hostExtent = dotDiameter + 4;
    m_presenceBadge->setFixedSize(hostExtent, hostExtent);
    m_presenceBadge->setBeaconDiameter(dotDiameter);
    m_presenceBadge->setCustomBackgroundColor(QColor());

    switch (m_presence) {
    case PresenceStatus::Available:
        m_presenceBadge->setStatus(InfoBadge::InfoBadgeStatus::Success);
        break;
    case PresenceStatus::Away:
        m_presenceBadge->setStatus(InfoBadge::InfoBadgeStatus::Caution);
        break;
    case PresenceStatus::Busy:
    case PresenceStatus::DoNotDisturb:
        m_presenceBadge->setStatus(InfoBadge::InfoBadgeStatus::Critical);
        break;
    case PresenceStatus::Offline:
        m_presenceBadge->setStatus(InfoBadge::InfoBadgeStatus::Attention);
        m_presenceBadge->setCustomBackgroundColor(
            themeColorsRef().textDisabled);
        break;
    case PresenceStatus::None:
        break;
    }

    m_presenceBadge->move(
        qMax(0, width() - hostExtent),
        qMax(0, height() - hostExtent));
    m_presenceBadge->raise();
}

void Avatar::updateFixedExtent()
{
    const int extent = avatarExtent();
    setFixedSize(extent, extent);
}

} // namespace fluent::status_info
