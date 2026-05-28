#include "RatingControl.h"
#include <QPainter>
#include <QMouseEvent>
#include "design/Typography.h"

namespace view::basicinput {

RatingControl::RatingControl(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());
}

void RatingControl::onThemeUpdated()
{
    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());
    update();
}

// ── 属性 setter ─────────────────────────────────────────────────────────────

void RatingControl::setValue(double value)
{
    value = qBound(-1.0, value, static_cast<double>(m_maxRating));
    if (qFuzzyCompare(m_value, value)) return;
    m_value = value;
    update();
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
    emit captionChanged(m_caption);
}

void RatingControl::setIsClearEnabled(bool enabled)
{
    if (m_isClearEnabled == enabled) return;
    m_isClearEnabled = enabled;
    emit isClearEnabledChanged(m_isClearEnabled);
}

void RatingControl::setIsReadOnly(bool readOnly)
{
    if (m_isReadOnly == readOnly) return;
    m_isReadOnly = readOnly;
    setCursor(readOnly ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
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

void RatingControl::setFontRole(const QString& role)
{
    if (m_fontRole == role) return;
    m_fontRole = role;
    setFont(themeFont(m_fontRole).toQFont());
    updateGeometry();
    update();
    emit fontRoleChanged();
}

void RatingControl::setCaptionFontRole(const QString& role)
{
    if (m_captionFontRole == role) return;
    m_captionFontRole = role;
    updateGeometry();
    update();
    emit captionFontRoleChanged();
}

// ── 几何辅助 ─────────────────────────────────────────────────────────────────

QSize RatingControl::iconCellSize() const
{
    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(m_starSize);
    QFontMetrics fm(iconFont);
    int w = fm.horizontalAdvance(Typography::Icons::FavoriteStar);
    int h = fm.height();
    return QSize(qMax(w, m_starSize), qMax(h, m_starSize));
}

QRectF RatingControl::starRect(int index) const
{
    QSize cell = iconCellSize();
    double x = index * (cell.width() + m_itemSpacing);
    return QRectF(x, 0, cell.width(), cell.height());
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

// ── 鼠标 → 评分值映射 ───────────────────────────────────────────────────────

double RatingControl::ratingFromPosition(int x) const
{
    for (int i = 0; i < m_maxRating; ++i) {
        QRectF r = starRect(i);
        if (x >= r.left() && x <= r.right()) {
            double midX = r.center().x();
            return (x < midX) ? (i + 0.5) : (i + 1.0);
        }
    }
    if (x > starRect(m_maxRating - 1).right())
        return m_maxRating;
    return 0;
}

// ── 绘制 ─────────────────────────────────────────────────────────────────────

void RatingControl::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = themeColors();

    // 确定要显示的值
    bool isHoverPreview = m_isHovered && !m_isReadOnly && m_hoverValue > 0;
    double displayValue = isHoverPreview
        ? m_hoverValue
        : (m_value >= 0 ? m_value : m_placeholderValue);
    bool isPlaceholder = (m_value < 0 && !isHoverPreview);
    bool isDisabled = !isEnabled();

    // 图标字体
    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(m_starSize);
    p.setFont(iconFont);

    // 状态颜色
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

    // 逐星绘制
    for (int i = 0; i < m_maxRating; ++i) {
        QRectF rect = starRect(i);
        double fillFraction = qBound(0.0, displayValue - i, 1.0);

        if (fillFraction >= 1.0) {
            p.setPen(filledColor);
            p.drawText(rect, Qt::AlignCenter, Typography::Icons::FavoriteStarFill);
        } else if (fillFraction <= 0.0) {
            p.setPen(emptyColor);
            p.drawText(rect, Qt::AlignCenter, Typography::Icons::FavoriteStar);
        } else {
            // 部分填充：先画空心，再用裁剪区域画实心
            p.setPen(emptyColor);
            p.drawText(rect, Qt::AlignCenter, Typography::Icons::FavoriteStar);
            p.save();
            p.setClipRect(QRectF(rect.left(), rect.top(),
                                  rect.width() * fillFraction, rect.height()));
            p.setPen(filledColor);
            p.drawText(rect, Qt::AlignCenter, Typography::Icons::FavoriteStarFill);
            p.restore();
        }
    }

    // 标题文字
    if (!m_caption.isEmpty()) {
        QFont captionFont = themeFont(m_captionFontRole).toQFont();
        p.setFont(captionFont);
        p.setPen(isDisabled ? c.textDisabled : c.textSecondary);
        int captionX = starsAreaWidth() + m_itemSpacing * 2;
        QRect captionRect(captionX, 0, width() - captionX, height());
        p.drawText(captionRect, Qt::AlignVCenter | Qt::AlignLeft, m_caption);
    }
}

// ── 鼠标交互 ─────────────────────────────────────────────────────────────────

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
    if (event->button() == Qt::LeftButton && !m_isReadOnly) {
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

} // namespace view::basicinput
