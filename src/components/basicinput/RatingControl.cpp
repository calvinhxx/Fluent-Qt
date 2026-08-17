#include "RatingControl.h"
#include "components/basicinput/private/BasicValueAccessibility_p.h"
#include "components/foundation/private/ValueAccessibility_p.h"
#include "design/Typography.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QtMath>

namespace fluent::basicinput {

namespace {

QPainterPath ratingStarPath(const QRectF& cell, int requestedSize)
{
    constexpr qreal kPi = 3.14159265358979323846;
    const qreal diameter = qMin<qreal>(qMax(1, requestedSize),
                                      qMin(cell.width(), cell.height()));
    const qreal outerRadius = diameter * 0.47;
    const qreal innerRadius = outerRadius * 0.48;
    const QPointF center = cell.center();

    QPainterPath path;
    for (int point = 0; point < 10; ++point) {
        const qreal radius = (point % 2 == 0) ? outerRadius : innerRadius;
        const qreal angle = -kPi / 2.0 + point * kPi / 5.0;
        const QPointF vertex(center.x() + qCos(angle) * radius,
                             center.y() + qSin(angle) * radius);
        if (point == 0)
            path.moveTo(vertex);
        else
            path.lineTo(vertex);
    }
    path.closeSubpath();
    return path;
}

void drawRatingStar(QPainter& painter, const QPainterPath& path,
                    const QColor& color, bool filled, qreal outlineWidth)
{
    painter.save();
    painter.setBrush(filled ? QBrush(color) : Qt::NoBrush);
    if (filled) {
        painter.setPen(Qt::NoPen);
    } else {
        QPen pen(color, outlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
    }
    painter.drawPath(path);
    painter.restore();
}

} // namespace

RatingControl::RatingControl(QWidget* parent)
    : QWidget(parent)
{
    detail::ensureBasicValueAccessibilityFactory();
    setAttribute(Qt::WA_Hover);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());
}

void RatingControl::onThemeUpdated()
{
    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());
    update();
}

// ── Property setters. zh_CN: 属性 setter ────────────────────────────────────

void RatingControl::setValue(double value)
{
    value = qBound(-1.0, value, static_cast<double>(m_maxRating));
    if (qFuzzyCompare(m_value, value)) return;
    m_value = value;
    update();
    accessibility::detail::notifyValueAccessibilityValue(
        this, qMax(0.0, m_value));
    emit valueChanged(m_value);
}

void RatingControl::setPlaceholderValue(double value)
{
    value = qBound(0.0, value, static_cast<double>(m_maxRating));
    if (qFuzzyCompare(m_placeholderValue, value)) return;
    m_placeholderValue = value;
    update();
    emit placeholderValueChanged(m_placeholderValue);
}

void RatingControl::setCaption(const QString& caption)
{
    if (m_caption == caption) return;
    m_caption = caption;
    updateGeometry();
    update();
    accessibility::detail::notifyValueAccessibilityText(
        this, QAccessible::DescriptionChanged);
    emit captionChanged(m_caption);
}

void RatingControl::setIsClearEnabled(bool enabled)
{
    if (m_isClearEnabled == enabled) return;
    m_isClearEnabled = enabled;
    accessibility::detail::notifyValueAccessibilityValue(
        this, qMax(0.0, m_value));
    emit isClearEnabledChanged(m_isClearEnabled);
}

void RatingControl::setIsReadOnly(bool readOnly)
{
    if (m_isReadOnly == readOnly) return;
    m_isReadOnly = readOnly;
    setCursor(readOnly ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
    QAccessible::State changed;
    changed.readOnly = true;
    changed.editable = true;
    accessibility::detail::notifyValueAccessibilityState(this, changed);
    emit isReadOnlyChanged(m_isReadOnly);
}

void RatingControl::setMaxRating(int rating)
{
    rating = qMax(1, rating);
    if (m_maxRating == rating) return;
    m_maxRating = rating;
    if (m_value > m_maxRating) m_value = m_maxRating;
    if (m_placeholderValue > m_maxRating) m_placeholderValue = m_maxRating;
    updateGeometry();
    update();
    accessibility::detail::notifyValueAccessibilityValue(
        this, qMax(0.0, m_value));
    emit maxRatingChanged(m_maxRating);
}

void RatingControl::setStarSize(int size)
{
    if (m_starSize == size) return;
    m_starSize = size;
    updateGeometry();
    update();
    emit starSizeChanged(m_starSize);
}

void RatingControl::setFontRole(Typography::FontRole role)
{
    if (m_fontRole == role) return;
    m_fontRole = role;
    setFont(themeFont(m_fontRole).toQFont());
    updateGeometry();
    update();
    emit fontRoleChanged();
}

void RatingControl::setCaptionFontRole(Typography::FontRole role)
{
    if (m_captionFontRole == role) return;
    m_captionFontRole = role;
    updateGeometry();
    update();
    emit captionFontRoleChanged();
}

// ── Geometry helpers. zh_CN: 几何辅助 ────────────────────────────────────────

QSize RatingControl::iconCellSize() const
{
    const QFont iconFont = Typography::Icons::font(m_starSize);
    QFontMetrics fm(iconFont);
    const QString starGlyph = Typography::Icons::glyphForSize(
        Typography::Icons::FavoriteStar, m_starSize);
    int w = fm.horizontalAdvance(starGlyph);
    int h = fm.height();
    return QSize(qMax(w, m_starSize), qMax(h, m_starSize));
}

QRectF RatingControl::starRect(int index) const
{
    QSize cell = iconCellSize();
    const int x = index * (cell.width() + m_itemSpacing);
    const QRect logicalRect(x, 0, cell.width(), cell.height());
    return QStyle::visualRect(layoutDirection(), rect(), logicalRect);
}

int RatingControl::starsAreaWidth() const
{
    int cellW = iconCellSize().width();
    return m_maxRating * cellW + (m_maxRating - 1) * m_itemSpacing;
}

QSize RatingControl::sizeHint() const
{
    QSize cell = iconCellSize();
    int w = starsAreaWidth();
    int h = cell.height();

    if (!m_caption.isEmpty()) {
        QFontMetrics fm(font());
        w += m_itemSpacing * 2 + fm.horizontalAdvance(m_caption);
        h = qMax(h, fm.height());
    }

    return QSize(w, h);
}

QSize RatingControl::minimumSizeHint() const
{
    return QSize(starsAreaWidth(), iconCellSize().height());
}

// ── Mouse-to-rating mapping. zh_CN: 鼠标 → 评分值映射 ───────────────────────

double RatingControl::ratingFromPosition(int x) const
{
    for (int i = 0; i < m_maxRating; ++i) {
        QRectF r = starRect(i);
        if (x >= r.left() && x <= r.right()) {
            double midX = r.center().x();
            const bool firstHalf = layoutDirection() == Qt::RightToLeft
                ? x > midX
                : x < midX;
            return firstHalf ? (i + 0.5) : (i + 1.0);
        }
    }
    const QRectF maximumStar = starRect(m_maxRating - 1);
    if ((layoutDirection() == Qt::LeftToRight && x > maximumStar.right())
        || (layoutDirection() == Qt::RightToLeft && x < maximumStar.left())) {
        return m_maxRating;
    }
    return 0;
}

double RatingControl::keyboardStepTarget(int direction) const
{
    constexpr double kStep = 0.5;
    const double current = m_value >= 0.0 ? m_value : 0.0;
    const double candidate = current + direction * kStep;
    if (candidate <= 0.0)
        return m_isClearEnabled ? -1.0 : kStep;
    return qMin(candidate, static_cast<double>(m_maxRating));
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void RatingControl::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = themeColorsRef();

    // Resolve the displayed value. zh_CN: 确定要显示的值。
    bool isHoverPreview = m_isHovered && !m_isReadOnly && m_hoverValue > 0;
    double displayValue = isHoverPreview
        ? m_hoverValue
        : (m_value >= 0 ? m_value : m_placeholderValue);
    bool isPlaceholder = (m_value < 0 && !isHoverPreview);
    bool isDisabled = !isEnabled();

    // State colors. zh_CN: 状态颜色。
    QColor filledColor, emptyColor;
    if (isDisabled) {
        filledColor = c.textDisabled;
        emptyColor = c.textDisabled;
    } else if (isPlaceholder) {
        filledColor = c.accentDisabled;
        emptyColor = c.strokeSecondary;
    } else if (isHoverPreview) {
        filledColor = c.accentSecondary;
        emptyColor = c.strokeSecondary;
    } else {
        filledColor = c.accentDefault;
        emptyColor = c.strokeSecondary;
    }

    // Paint star by star. zh_CN: 逐星绘制。
    for (int i = 0; i < m_maxRating; ++i) {
        QRectF rect = starRect(i);
        const QPainterPath star = ratingStarPath(rect, m_starSize);
        const qreal outlineWidth = qMax<qreal>(1.25, m_starSize / 12.0);
        double fillFraction = qBound(0.0, displayValue - i, 1.0);

        if (fillFraction >= 1.0) {
            drawRatingStar(p, star, filledColor, true, outlineWidth);
        } else if (fillFraction <= 0.0) {
            drawRatingStar(p, star, emptyColor, false, outlineWidth);
        } else {
            // Partial fill: paint the outline, then the solid star inside a clip.
            // zh_CN: 部分填充——先画空心，再用裁剪区域画实心。
            drawRatingStar(p, star, emptyColor, false, outlineWidth);
            p.save();
            const qreal fillWidth = rect.width() * fillFraction;
            const qreal fillX = layoutDirection() == Qt::RightToLeft
                ? rect.right() - fillWidth
                : rect.left();
            p.setClipRect(QRectF(fillX, rect.top(), fillWidth, rect.height()));
            drawRatingStar(p, star, filledColor, true, outlineWidth);
            p.restore();
        }
    }

    // Caption text. zh_CN: 标题文字。
    if (!m_caption.isEmpty()) {
        QFont captionFont = themeFont(m_captionFontRole).toQFont();
        p.setFont(captionFont);
        p.setPen(isDisabled ? c.textDisabled : c.textSecondary);
        const int captionX = starsAreaWidth() + m_itemSpacing * 2;
        const QRect logicalCaptionRect(captionX, 0, width() - captionX, height());
        const QRect captionRect =
            QStyle::visualRect(layoutDirection(), rect(), logicalCaptionRect);
        p.drawText(captionRect,
                   QStyle::visualAlignment(layoutDirection(),
                                           Qt::AlignVCenter | Qt::AlignLeft),
                   m_caption);
    }

    if (!isDisabled && hasFocus() && m_keyboardFocusVisible) {
        QColor focusColor = c.textSecondary;
        focusColor.setAlpha(120);
        const QRect logicalFocusRect(0, 0, starsAreaWidth(), height());
        const QRect focusRect =
            QStyle::visualRect(layoutDirection(), rect(), logicalFocusRect)
                .adjusted(1, 1, -1, -1);
        p.setPen(QPen(focusColor, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(focusRect, themeRadius().control, themeRadius().control);
    }
}

// ── Mouse interaction. zh_CN: 鼠标交互 ───────────────────────────────────────

void RatingControl::enterEvent(FluentEnterEvent* event)
{
    m_isHovered = true;
    QWidget::enterEvent(event);
}

void RatingControl::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    m_hoverValue = -1.0;
    update();
    QWidget::leaveEvent(event);
}

void RatingControl::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_isReadOnly) {
        double newHoverValue = ratingFromPosition(event->pos().x());
        if (!qFuzzyCompare(m_hoverValue, newHoverValue)) {
            m_hoverValue = newHoverValue;
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void RatingControl::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        if (!hasFocus())
            setFocus(Qt::MouseFocusReason);
        m_keyboardFocusVisible = false;
        if (!m_isReadOnly)
            m_isPressed = true;
    }
    QWidget::mousePressEvent(event);
}

void RatingControl::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isPressed && !m_isReadOnly) {
        m_isPressed = false;
        double clickValue = ratingFromPosition(event->pos().x());
        if (clickValue > 0) {
            if (m_isClearEnabled && qFuzzyCompare(clickValue, m_value)) {
                setValue(-1.0);
            } else {
                setValue(clickValue);
            }
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void RatingControl::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled() || m_isReadOnly) {
        QWidget::keyPressEvent(event);
        return;
    }

    m_keyboardFocusVisible = true;
    update();

    int direction = 0;
    switch (event->key()) {
    case Qt::Key_Left:
        direction = layoutDirection() == Qt::RightToLeft ? 1 : -1;
        break;
    case Qt::Key_Right:
        direction = layoutDirection() == Qt::RightToLeft ? -1 : 1;
        break;
    case Qt::Key_Down:
        direction = -1;
        break;
    case Qt::Key_Up:
        direction = 1;
        break;
    case Qt::Key_Home:
        setValue(m_isClearEnabled ? -1.0 : 0.5);
        event->accept();
        return;
    case Qt::Key_End:
        setValue(m_maxRating);
        event->accept();
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    setValue(keyboardStepTarget(direction));
    event->accept();
}

void RatingControl::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason)
        m_keyboardFocusVisible = false;
    else if (event->reason() == Qt::TabFocusReason
             || event->reason() == Qt::BacktabFocusReason
             || event->reason() == Qt::ShortcutFocusReason)
        m_keyboardFocusVisible = true;
    update();
}

void RatingControl::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

} // namespace fluent::basicinput
