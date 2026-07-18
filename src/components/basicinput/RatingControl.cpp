#include "RatingControl.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include "design/Typography.h"

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

// ── Property setters. zh_CN: 属性 setter ────────────────────────────────────

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

// ── Mouse-to-rating mapping. zh_CN: 鼠标 → 评分值映射 ───────────────────────

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

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void RatingControl::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = themeColors();

    // Resolve the displayed value. zh_CN: 确定要显示的值。
    bool isHoverPreview = m_isHovered && !m_isReadOnly && m_hoverValue > 0;
    double displayValue = isHoverPreview
        ? m_hoverValue
        : (m_value >= 0 ? m_value : m_placeholderValue);
    bool isPlaceholder = (m_value < 0 && !isHoverPreview);
    bool isDisabled = !isEnabled();

    // Design-language-aware UNFILLED star tone. A star is geometrically design-neutral, so the only
    // honest delta across languages is the placeholder/empty stroke and the hover-preview tint — not
    // size or spacing. Fluent keeps its WinUI strokeSecondary outline; Material 3 reads the empty star
    // as the stronger neutral on-surface "outline" (strokeStrong, the same token M3 buttons use for
    // their 1dp outline); macOS uses a dim secondary-text neutral. The filled-star color stays the
    // accent in every language. Guard against the invalid-QColor trap by sourcing real tokens only.
    // zh_CN: 设计语言感知的「未填充星」色调。星形本身与设计语言无关,各语言唯一诚实的差异是占位/空心描边
    // 与 hover 预览着色——而非尺寸或间距。Fluent 保留其 WinUI strokeSecondary 描边;Material 3 将空心星
    // 读作更强的中性 on-surface「outline」(strokeStrong,即 M3 按钮 1dp 描边所用 token);macOS 用偏暗的
    // 次要文本中性色。填充星色在所有语言下均保持 accent。仅取真实 token,规避无效 QColor 陷阱。
    const DesignLanguage lang = themeDesignLanguage();
    const bool darkTheme = effectiveTheme() == Dark;
    // Theme-aware veil: darkens light surfaces / lightens dark ones so a hover tint stays visible in
    // both App themes. zh_CN: 主题感知薄层:浅色面变暗、深色面变亮,使 hover 着色在明暗两主题下都可见。
    const auto veil = [darkTheme](int a) {
        return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a);
    };
    // Alpha-composite a (semi-transparent) veil over a base color — used to tint a glyph pen, since the
    // stars are painted as text, not fills. zh_CN: 将(半透明)veil 叠加到基色上——星形以文字绘制,故用于着色画笔。
    const auto blend = [](QColor base, QColor over) {
        const double oa = over.alpha() / 255.0;
        base.setRed(qRound(base.red() * (1.0 - oa) + over.red() * oa));
        base.setGreen(qRound(base.green() * (1.0 - oa) + over.green() * oa));
        base.setBlue(qRound(base.blue() * (1.0 - oa) + over.blue() * oa));
        return base;
    };
    // The empty/unfilled outline tone, per design language. zh_CN: 各设计语言下的空心/未填充描边色。
    QColor emptyTone = c.strokeSecondary;                 // DesignFluent: unchanged. zh_CN: Fluent 不变。
    if (lang == DesignMaterial) {
        emptyTone = c.strokeStrong;                       // M3 neutral "outline". zh_CN: M3 中性 outline。
    } else if (lang == DesignCupertino) {
        emptyTone = c.textSecondary;                      // macOS dim neutral. zh_CN: macOS 偏暗中性。
    }

    // State colors. zh_CN: 状态颜色。
    QColor filledColor, emptyColor;
    if (isDisabled) {
        filledColor = c.textDisabled;
        emptyColor = c.textDisabled;
    } else if (isPlaceholder) {
        filledColor = c.accentDisabled;
        emptyColor = (lang == DesignFluent) ? c.strokeSecondary : emptyTone;
    } else if (isHoverPreview) {
        // Hover preview. Fluent keeps its accentSecondary glyph; M3/macOS tint the accent with the
        // theme-aware veil so the in-progress selection reads as an interactive preview, not a commit.
        // zh_CN: Hover 预览。Fluent 保留 accentSecondary 字形;M3/macOS 用主题感知 veil 给 accent 着色,
        // 使进行中的选择读作交互预览而非已提交。
        filledColor = (lang == DesignFluent) ? c.accentSecondary
                                             : blend(c.accentDefault, veil(0x33));
        emptyColor = (lang == DesignFluent) ? c.strokeSecondary : emptyTone;
    } else {
        filledColor = c.accentDefault;
        emptyColor = (lang == DesignFluent) ? c.strokeSecondary : emptyTone;
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
            p.setClipRect(QRectF(rect.left(), rect.top(),
                                  rect.width() * fillFraction, rect.height()));
            drawRatingStar(p, star, filledColor, true, outlineWidth);
            p.restore();
        }
    }

    // Caption text. zh_CN: 标题文字。
    if (!m_caption.isEmpty()) {
        QFont captionFont = themeFont(m_captionFontRole).toQFont();
        p.setFont(captionFont);
        p.setPen(isDisabled ? c.textDisabled : c.textSecondary);
        int captionX = starsAreaWidth() + m_itemSpacing * 2;
        QRect captionRect(captionX, 0, width() - captionX, height());
        p.drawText(captionRect, Qt::AlignVCenter | Qt::AlignLeft, m_caption);
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

} // namespace fluent::basicinput
