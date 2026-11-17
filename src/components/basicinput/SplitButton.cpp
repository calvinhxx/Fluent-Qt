#include "SplitButton.h"
#include "private/ButtonContentPainter_p.h"
#include "components/basicinput/private/MenuButtonAccessibility_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimer>
#include <QVariantAnimation>
#include <QtMath>

namespace fluent::basicinput {

namespace {

int splitButtonTextWidth(const QFontMetrics& fm, const QString& text)
{
    if (text.isEmpty())
        return 0;
    // Ink can extend past horizontalAdvance; take the widest metric so sizeHint matches painted glyphs.
    const int advance = fm.horizontalAdvance(text);
    const int bounds = fm.boundingRect(text).width();
    const int tight = fm.tightBoundingRect(text).width();
    return qMax(advance, qMax(bounds, tight));
}

} // namespace

SplitButton::SplitButton(const QString& text, QWidget* parent) : Button(text, parent)
{
    detail::initializeMenuButtonAccessibility(this);
    setMouseTracking(true);
    m_pressAnimation = new QVariantAnimation(this);
    connect(m_pressAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_pressProgress = value.toReal();
        update();
    });
    connect(m_pressAnimation, &QVariantAnimation::finished, this, [this]() {
        m_pressProgress = 0.0;
        m_animatedPart = None;
        update();
    });
}

SplitButton::~SplitButton()
{
    if (m_menu)
        disconnect(m_menu.data(), nullptr, this, nullptr);
}

void SplitButton::startPressAnimation(SplitPart part)
{
    if (!m_pressAnimation || part == None)
        return;
    m_animatedPart = part;
    m_pressAnimation->stop();
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
    m_pressAnimation->setStartValue(0.0);
    m_pressAnimation->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_pressAnimation, themeAnimation().slow);
}

void SplitButton::setMenu(QMenu* menu)
{
    if (m_menu == menu)
        return;

    const bool hadMenu = m_menu != nullptr;
    if (m_menu)
        disconnect(m_menu.data(), nullptr, this, nullptr);
    setOpen(false);
    m_menu = menu;
    if (m_menu) {
        connect(m_menu, &QMenu::aboutToShow, this, [this]() {
            // Empty QMenu stays hidden without emitting aboutToHide; recheck after one event-loop turn.
            QTimer::singleShot(0, this, [this, menu = m_menu] {
                if (menu && menu == m_menu && menu->isEmpty() && !menu->isVisible())
                    setOpen(false);
            });
            setOpen(true);
        });
        connect(m_menu, &QMenu::aboutToHide, this, [this]() { setOpen(false); });
        connect(m_menu, &QObject::destroyed, this, [this]() {
            setOpen(false);
            detail::notifyMenuButtonMenuAccessibility(this, true);
            emit menuChanged();
        });
        setOpen(m_menu->isVisible());
    }
    detail::notifyMenuButtonMenuAccessibility(this, hadMenu != (m_menu != nullptr));
    emit menuChanged();
}

void SplitButton::setOpen(bool open)
{
    if (m_isOpen == open)
        return;
    m_isOpen = open;
    update();
    detail::notifyMenuButtonOpenAccessibility(this);
    emit openChanged();
}

void SplitButton::setSecondaryWidth(int width)
{
    if (m_secondaryWidth != width) {
        m_secondaryWidth = width;
        updateGeometry();
        update();
        emit secondaryWidthChanged();
    }
}

void SplitButton::mouseMoveEvent(QMouseEvent* event)
{
    SplitPart part = getPartAt(event->pos());
    if (m_hoverPart != part) {
        m_hoverPart = part;
        update();
    }
    Button::mouseMoveEvent(event);
}

void SplitButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressPart = getPartAt(event->pos());
        startPressAnimation(m_pressPart);
        update();
        if (m_pressPart == Secondary) {
            event->accept();
            return;
        }
    }
    Button::mousePressEvent(event);
}

void SplitButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const SplitPart releasePart = getPartAt(event->pos());

        if (m_pressPart == Secondary) {
            event->accept();
            if (releasePart == Secondary && m_menu)
                detail::showMenuButtonMenu(this);
            m_pressPart = None;
            update();
            return;
        }

        if (m_pressPart == Primary && releasePart != Primary) {
            // Releasing outside the primary zone cancels the primary command.
            m_pressPart = None;
            setDown(false);
            update();
            event->accept();
            return;
        }

        m_pressPart = None;
        update();
    }
    Button::mouseReleaseEvent(event);
}

void SplitButton::keyPressEvent(QKeyEvent* event)
{
    const bool altDown = event->key() == Qt::Key_Down && event->modifiers().testFlag(Qt::AltModifier);
    const bool f4 = event->key() == Qt::Key_F4 && event->modifiers() == Qt::NoModifier;
    if (m_menu && (altDown || f4)) {
        detail::showMenuButtonMenu(this);
        event->accept();
        return;
    }
    Button::keyPressEvent(event);
}

void SplitButton::leaveEvent(QEvent* event)
{
    m_hoverPart = None;
    m_pressPart = None;
    update();
    Button::leaveEvent(event);
}

SplitButton::SplitPart SplitButton::getPartAt(const QPoint& pos) const
{
    if (!rect().contains(pos))
        return None;

    const QRect logicalSecondary(width() - m_secondaryWidth, 0, m_secondaryWidth, height());
    const QRect secondary = QStyle::visualRect(layoutDirection(), rect(), logicalSecondary);
    if (secondary.contains(pos))
        return Secondary;
    return Primary;
}

int SplitButton::primaryTrailingInset() const
{
    const auto& spacing = themeSpacing();
    return (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;
}

QRectF SplitButton::primaryContentRect(const QRectF& primaryRect) const
{
    const qreal inset = qMin(primaryRect.width(), static_cast<qreal>(primaryTrailingInset()));
    return layoutDirection() == Qt::RightToLeft
        ? primaryRect.adjusted(inset, 0, 0, 0)
        : primaryRect.adjusted(0, 0, -inset, 0);
}

QSize SplitButton::sizeHint() const
{
    if (fluentLayout() == IconAbove || fluentLayout() == IconBelow) {
        const QSize primary = Button::sizeHint();
        return QSize(primary.width() + m_secondaryWidth + (text().isEmpty() ? 0 : primaryTrailingInset()),
            primary.height());
    }
    const auto& spacing = themeSpacing();
    QFontMetrics fm(font());

    const int hPadding =
        (fluentSize() == Small) ? spacing.small : (fluentSize() == Large ? spacing.standard : spacing.padding.controlH);
    const int vPadding = (fluentSize() == Small)
        ? spacing.gap.tight
        : (fluentSize() == Large ? spacing.small : spacing.padding.controlV);
    const int iconGap = (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;

    const bool iconOnly = fluentLayout() == IconOnly;
    const QString txt = iconOnly ? QString() : text();
    const bool hasIconFont = fluentLayout() != TextOnly && !iconGlyph().isEmpty();
    const int txtWidth = splitButtonTextWidth(fm, txt);
    const int iconWidth = hasIconFont ? iconPixelSize() : 0;
    const int contentWidth = txtWidth + iconWidth + ((!txt.isEmpty() && hasIconFont) ? iconGap : 0);
    const int primaryWidth = qMax(Button::sizeHint().width(), contentWidth + hPadding * 2);
    // Trailing inset clears text from the divider; icon-only centers within the primary zone.
    const int trailing = iconOnly ? 0 : primaryTrailingInset();

    return QSize(primaryWidth + m_secondaryWidth + trailing, fm.height() + vPadding * 2);
}

QSize SplitButton::minimumSizeHint() const
{
    return sizeHint();
}

void SplitButton::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();

    int sWidth = m_secondaryWidth;
    int sepMargin = spacing.gap.tight;
    const int chevronSize = Typography::IconSize::Compact;

    QRectF fullRect = rect();
    const QRect logicalPrimaryRect(0, 0, width() - sWidth, height());
    const QRect logicalSecondaryRect(width() - sWidth, 0, sWidth, height());
    const QRectF primaryRect = QStyle::visualRect(layoutDirection(), rect(), logicalPrimaryRect);
    const QRectF secondaryRect = QStyle::visualRect(layoutDirection(), rect(), logicalSecondaryRect);
    const qreal dividerX = layoutDirection() == Qt::RightToLeft ? sWidth : width() - sWidth;

    const bool checked = isChecked();
    const bool accentLike = (fluentStyle() == Accent || checked);

    QColor textColor = accentLike ? colors.textOnAccent : colors.textPrimary;
    QColor chevronColor = textColor;
    if (!isEnabled()) {
        textColor = colors.textDisabled;
        chevronColor = colors.textDisabled;
    }

    QColor baseBg;

    if (fluentStyle() == Accent || checked) {
        baseBg = colors.accentDefault;
        textColor = colors.textOnAccent;
    } else if (fluentStyle() == Subtle) {
        baseBg = Qt::transparent;
        textColor = colors.textPrimary;
    } else {
        baseBg = colors.controlDefault;
        textColor = colors.textPrimary;
    }

    if (!isEnabled()) {
        baseBg = fluentStyle() == Subtle && !checked ? QColor(Qt::transparent) : colors.controlDisabled;
        textColor = colors.textDisabled;
    }
    chevronColor = textColor;

    painter.setPen(Qt::NoPen);
    painter.setBrush(baseBg);
    painter.drawRoundedRect(fullRect, radius.control, radius.control);

    if (isEnabled()) {
        auto drawHighlight = [&](const QRectF& r, SplitPart part) {
            QColor highlight;
            if (m_pressPart == part) {
                highlight = (fluentStyle() == Accent || checked) ? colors.accentTertiary : colors.controlTertiary;
            } else if (m_hoverPart == part) {
                highlight = (fluentStyle() == Accent || checked) ? colors.accentSecondary : colors.controlSecondary;
            } else {
                return;
            }
            painter.setBrush(highlight);
            painter.save();
            painter.setClipRect(r);
            painter.drawRoundedRect(fullRect, radius.control, radius.control);
            painter.restore();
        };

        drawHighlight(primaryRect, Primary);
        drawHighlight(secondaryRect, Secondary);
    }

    if (isEnabled()) {
        QColor sepColor = (fluentStyle() == Accent || checked) ? colors.strokeDivider : colors.strokeDefault;
        painter.setPen(QPen(sepColor, 1));
        painter.drawLine(QPointF(dividerX, sepMargin), QPointF(dividerX, height() - sepMargin));
    }

    painter.setPen(textColor);
    painter.setFont(font());
    // Apply press rebound independently to each split zone.
    constexpr qreal kPi = 3.14159265358979323846;
    const qreal rebound = qSin(qBound<qreal>(0.0, m_pressProgress, 1.0) * kPi);
    const double primaryOffset = (m_pressPart == Primary ? 0.5 : 0.0)
        + (m_animatedPart == Primary ? rebound * 2.0 : 0.0);
    const double secondaryOffset = (m_pressPart == Secondary ? 0.5 : 0.0)
        + (m_animatedPart == Secondary ? rebound * 2.0 : 0.0);

    if (fluentLayout() == IconAbove || fluentLayout() == IconBelow) {
        const QRectF layoutRect = text().isEmpty() ? primaryRect : primaryContentRect(primaryRect);
        paintStackedContent(painter, layoutRect.translated(0, primaryOffset));
    } else {
        const bool iconOnly = fluentLayout() == IconOnly;
        QRectF layoutRect = iconOnly ? primaryRect : primaryContentRect(primaryRect);
        const int hPadding = fluentSize() == Small
            ? spacing.small
            : (fluentSize() == Large ? spacing.standard : spacing.padding.controlH);
        if (!iconOnly) {
            if (layoutDirection() == Qt::RightToLeft)
                layoutRect.adjust(0, 0, -hPadding, 0);
            else
                layoutRect.adjust(hPadding, 0, 0, 0);
        }
        painter.save();
        painter.setClipRect(iconOnly ? primaryRect : primaryContentRect(primaryRect), Qt::IntersectClip);
        ButtonContentPainter::paintHorizontalButtonContent(*this, painter, layoutRect.translated(0, primaryOffset),
            fluentSize() == Small ? spacing.gap.tight : spacing.gap.normal,
            iconOnly ? Qt::AlignHCenter : Qt::AlignLeading);
        painter.restore();
    }

    if (m_animatedPart == Secondary && rebound > 0.0)
        chevronColor.setAlphaF(chevronColor.alphaF() * (1.0 - 0.25 * rebound));
    painter.setPen(chevronColor);
    Typography::Icons::paintGlyph(painter, secondaryRect.translated(0, secondaryOffset), Typography::Icons::ChevronDown,
        chevronSize, Qt::AlignCenter);

    if (hasFocus() && isEnabled()) {
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120);
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(fullRect.adjusted(1.5, 1.5, -1.5, -1.5), radius.control - 1, radius.control - 1);
    }
}

} // namespace fluent::basicinput
