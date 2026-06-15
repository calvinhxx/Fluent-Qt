#include "Shimmer.h"

#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimerEvent>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

#include "design/Animation.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"

namespace fluent::status_info {
namespace {

constexpr int kAnimationIntervalMs = 16;
constexpr int kMinimumCycleDurationMs = Animation::Duration::Normal;
constexpr int kDefaultCycleDurationMs = Animation::Duration::VerySlow + Animation::Duration::VerySlow;
constexpr qreal kLineHeight = 12.0;
constexpr qreal kLineGap = 8.0;
constexpr qreal kAvatarSize = 32.0;

qreal normalizedProgress(qreal progress)
{
    if (!std::isfinite(progress))
        return 0.0;
    progress = std::fmod(progress, 1.0);
    return progress < 0.0 ? progress + 1.0 : progress;
}

QColor effectiveColor(const QColor& color, const QColor& fallback)
{
    return color.isValid() ? color : fallback;
}

QColor blendColor(const QColor& foreground, const QColor& background, qreal amount)
{
    const qreal t = qBound<qreal>(0.0, amount, 1.0);
    return QColor(qRound(foreground.red() * t + background.red() * (1.0 - t)),
                  qRound(foreground.green() * t + background.green() * (1.0 - t)),
                  qRound(foreground.blue() * t + background.blue() * (1.0 - t)),
                  qRound(foreground.alpha() * t + background.alpha() * (1.0 - t)));
}

QPainterPath elementPath(const ShimmerPainter::Element& element)
{
    QPainterPath path;
    if (!element.rect.isValid() || element.rect.isEmpty())
        return path;

    switch (element.shape) {
    case ShimmerPainter::Shape::Rectangle:
        path.addRect(element.rect);
        break;
    case ShimmerPainter::Shape::RoundedRect: {
        const qreal radius = element.radius >= 0.0 ? element.radius : CornerRadius::Control;
        path.addRoundedRect(element.rect, radius, radius);
        break;
    }
    case ShimmerPainter::Shape::Circle:
        path.addEllipse(element.rect);
        break;
    case ShimmerPainter::Shape::Line: {
        const qreal radius = element.radius >= 0.0 ? element.radius : element.rect.height() / 2.0;
        path.addRoundedRect(element.rect, radius, radius);
        break;
    }
    }
    return path;
}

QRectF boundedRect(const QRectF& bounds)
{
    return bounds.normalized().adjusted(0.5, 0.5, -0.5, -0.5);
}

} // namespace

ShimmerPainter::Palette ShimmerPainter::paletteFromTheme(
    const fluent::FluentElement::Colors& colors,
    bool enabled)
{
    Palette palette;
    const QColor canvas = effectiveColor(colors.bgCanvas, QColor(Qt::white));
    const bool darkSurface = canvas.lightness() < 96;
    const QColor base = darkSurface
        ? blendColor(QColor(Qt::white), canvas, 0.12)
        : blendColor(QColor(Qt::black), canvas, 0.075);
    const QColor fallbackBorder = effectiveColor(colors.strokeDefault, QColor(0, 0, 0, 18));

    palette.baseColor = enabled ? base : effectiveColor(colors.controlDisabled, QColor(0, 0, 0, 8));
    palette.highlightColor = darkSurface ? QColor(255, 255, 255, enabled ? 68 : 18)
                                         : QColor(255, 255, 255, enabled ? 218 : 50);
    palette.borderColor = fallbackBorder;
    return palette;
}

void ShimmerPainter::paint(QPainter* painter,
                           const QVector<Element>& elements,
                           const Palette& palette,
                           qreal progress,
                           bool animated)
{
    if (!painter || elements.isEmpty())
        return;

    QPainterPath combinedPath;
    QRectF bounds;
    for (const Element& element : elements) {
        if (!element.rect.isValid() || element.rect.isEmpty())
            continue;
        combinedPath.addPath(elementPath(element));
        bounds = bounds.isNull() ? element.rect : bounds.united(element.rect);
    }
    if (combinedPath.isEmpty() || bounds.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(palette.baseColor);
    painter->drawPath(combinedPath);

    if (animated && palette.highlightColor.alpha() > 0) {
        const qreal phase = normalizedProgress(progress);
        const qreal sweepWidth = qMax<qreal>(56.0, bounds.width() * 0.42);
        const qreal sweepX = bounds.left() - sweepWidth
            + (bounds.width() + sweepWidth * 2.0) * phase;

        QLinearGradient gradient(QPointF(sweepX - sweepWidth, bounds.center().y()),
                                 QPointF(sweepX + sweepWidth, bounds.center().y()));
        QColor transparent = palette.highlightColor;
        transparent.setAlpha(0);
        gradient.setColorAt(0.00, transparent);
        gradient.setColorAt(0.48, palette.highlightColor);
        gradient.setColorAt(1.00, transparent);

        painter->setClipPath(combinedPath);
        painter->fillRect(bounds.adjusted(-sweepWidth, 0.0, sweepWidth, 0.0), gradient);
        painter->setClipping(false);
    }

    if (palette.borderColor.alpha() > 0) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(palette.borderColor, 1.0));
        painter->drawPath(combinedPath);
    }

    painter->restore();
}

QVector<ShimmerPainter::Element> ShimmerPainter::imageCardElements(const QRectF& bounds,
                                                                   qreal radius)
{
    const QRectF surface = boundedRect(bounds);
    if (!surface.isValid() || surface.isEmpty())
        return {};
    return {Element(Shape::RoundedRect, surface, radius)};
}

QVector<ShimmerPainter::Element> ShimmerPainter::avatarTextRowElements(const QRectF& bounds)
{
    const QRectF surface = boundedRect(bounds);
    if (!surface.isValid() || surface.isEmpty())
        return {};

    const qreal avatar = qMin(kAvatarSize, qMax<qreal>(16.0, surface.height() - ::Spacing::Small));
    const QRectF avatarRect(surface.left(),
                            surface.center().y() - avatar / 2.0,
                            avatar,
                            avatar);
    const qreal textLeft = avatarRect.right() + ::Spacing::Medium;
    const qreal textWidth = qMax<qreal>(16.0, surface.right() - textLeft);
    const qreal firstLineY = surface.center().y() - kLineHeight - kLineGap / 2.0;
    const qreal secondLineY = surface.center().y() + kLineGap / 2.0;

    return {
        Element(Shape::Circle, avatarRect),
        Element(Shape::Line, QRectF(textLeft, firstLineY, textWidth * 0.78, kLineHeight)),
        Element(Shape::Line, QRectF(textLeft, secondLineY, textWidth * 0.52, kLineHeight))
    };
}

QVector<ShimmerPainter::Element> ShimmerPainter::textBlockElements(const QRectF& bounds,
                                                                   int lineCount)
{
    const QRectF surface = boundedRect(bounds);
    if (!surface.isValid() || surface.isEmpty() || lineCount <= 0)
        return {};

    QVector<Element> elements;
    elements.reserve(lineCount);
    const qreal lineStep = kLineHeight + kLineGap;
    const qreal totalHeight = lineCount * kLineHeight + (lineCount - 1) * kLineGap;
    qreal y = surface.top() + qMax<qreal>(0.0, (surface.height() - totalHeight) / 2.0);
    for (int line = 0; line < lineCount; ++line) {
        const qreal widthRatio = line == lineCount - 1 ? 0.62 : (line % 2 == 0 ? 0.92 : 0.76);
        elements.append(Element(Shape::Line,
                                QRectF(surface.left(), y, surface.width() * widthRatio, kLineHeight)));
        y += lineStep;
    }
    return elements;
}

Shimmer::Shimmer(QWidget* parent)
    : QWidget(parent)
    , m_cycleDuration(kDefaultCycleDurationMs)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("Shimmer"));
    updateThemePalette();
    updateTemplateElements();
}

Shimmer::~Shimmer()
{
    m_animationTimer.stop();
}

void Shimmer::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    updateAnimationState();
    update();
    emit activeChanged(m_active);
}

void Shimmer::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;
    m_animationEnabled = enabled;
    updateAnimationState();
    update();
    emit animationEnabledChanged(m_animationEnabled);
}

void Shimmer::setShimmerProgress(qreal progress)
{
    const qreal normalized = normalizedProgress(progress);
    if (qFuzzyCompare(m_progress + 1.0, normalized + 1.0))
        return;
    m_progress = normalized;
    update();
    emit shimmerProgressChanged(m_progress);
}

void Shimmer::setCycleDuration(int durationMs)
{
    const int boundedDuration = qMax(kMinimumCycleDurationMs, durationMs);
    if (m_cycleDuration == boundedDuration)
        return;
    m_cycleDuration = boundedDuration;
    updateAnimationState();
    emit cycleDurationChanged(m_cycleDuration);
}

void Shimmer::setShimmerTemplate(ShimmerTemplate templateKind)
{
    if (m_template == templateKind)
        return;
    m_template = templateKind;
    updateTemplateElements();
    updateGeometry();
    update();
    emit shimmerTemplateChanged(m_template);
}

void Shimmer::setElements(const QVector<ShimmerPainter::Element>& elements)
{
    m_customElements = elements;
    if (m_template != ShimmerTemplate::Custom) {
        m_template = ShimmerTemplate::Custom;
        emit shimmerTemplateChanged(m_template);
    }
    updateGeometry();
    update();
    emit elementsChanged();
}

void Shimmer::clearElements()
{
    if (m_customElements.isEmpty())
        return;
    m_customElements.clear();
    update();
    emit elementsChanged();
}

QSize Shimmer::sizeHint() const
{
    switch (m_template) {
    case ShimmerTemplate::ImageCard:
        return QSize(240, 140);
    case ShimmerTemplate::AvatarTextRow:
        return QSize(240, 56);
    case ShimmerTemplate::TextBlock:
    case ShimmerTemplate::Custom:
        return QSize(240, 72);
    }
    return QSize(240, 72);
}

QSize Shimmer::minimumSizeHint() const
{
    return QSize(32, 24);
}

void Shimmer::onThemeUpdated()
{
    updateThemePalette();
    update();
}

void Shimmer::paintEvent(QPaintEvent*)
{
    if (!m_active)
        return;

    QPainter painter(this);
    ShimmerPainter::paint(&painter, effectiveElements(), m_palette, m_progress,
                          m_active && m_animationEnabled && isEnabled());
}

void Shimmer::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_animationTimer.timerId()) {
        QWidget::timerEvent(event);
        return;
    }

    m_progress = normalizedProgress(m_progress + static_cast<qreal>(kAnimationIntervalMs) / m_cycleDuration);
    update();
    emit shimmerProgressChanged(m_progress);
}

void Shimmer::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateAnimationState();
}

void Shimmer::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    updateAnimationState();
}

void Shimmer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateTemplateElements();
}

void Shimmer::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        updateThemePalette();
        updateAnimationState();
        update();
    }
}

QVector<ShimmerPainter::Element> Shimmer::effectiveElements() const
{
    return m_template == ShimmerTemplate::Custom ? m_customElements : m_templateElements;
}

void Shimmer::updateTemplateElements()
{
    const QRectF bounds = QRectF(rect()).adjusted(::Spacing::Small, ::Spacing::Small,
                                                  -::Spacing::Small, -::Spacing::Small);
    switch (m_template) {
    case ShimmerTemplate::ImageCard:
        m_templateElements = ShimmerPainter::imageCardElements(bounds, CornerRadius::Control);
        break;
    case ShimmerTemplate::AvatarTextRow:
        m_templateElements = ShimmerPainter::avatarTextRowElements(bounds);
        break;
    case ShimmerTemplate::TextBlock:
        m_templateElements = ShimmerPainter::textBlockElements(bounds, 3);
        break;
    case ShimmerTemplate::Custom:
        break;
    }
}

void Shimmer::updateThemePalette()
{
    m_palette = ShimmerPainter::paletteFromTheme(themeColors(), isEnabled());
}

void Shimmer::updateAnimationState()
{
    const bool shouldRun = m_active && m_animationEnabled && isEnabled() && isVisible();
    if (shouldRun) {
        if (!m_animationTimer.isActive())
            m_animationTimer.start(kAnimationIntervalMs, this);
    } else if (m_animationTimer.isActive()) {
        m_animationTimer.stop();
    }
}

} // namespace fluent::status_info
