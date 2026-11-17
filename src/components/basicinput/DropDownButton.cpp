#include "DropDownButton.h"
#include "components/basicinput/private/MenuButtonAccessibility_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionButton>
#include <QTimer>
#include <QtMath>

namespace fluent::basicinput {

DropDownButton::DropDownButton(const QString& text, QWidget* parent) : Button(text, parent)
{
    detail::initializeMenuButtonAccessibility(this);
    initAnimation();
}

DropDownButton::DropDownButton(QWidget* parent) : Button(parent)
{
    detail::initializeMenuButtonAccessibility(this);
    initAnimation();
}

DropDownButton::~DropDownButton()
{
    if (m_menu)
        disconnect(m_menu.data(), nullptr, this, nullptr);
}

void DropDownButton::initAnimation()
{
    if (m_pressAnimation)
        return;
    m_pressAnimation = new QPropertyAnimation(this, "pressProgress", this);
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
}

void DropDownButton::setMenu(QMenu* menu)
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
            // Empty QMenu emits aboutToShow without aboutToHide; recheck after one event-loop turn.
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

void DropDownButton::setOpen(bool open)
{
    if (m_isOpen == open)
        return;
    m_isOpen = open;
    update();
    detail::notifyMenuButtonOpenAccessibility(this);
    emit openChanged();
}

void DropDownButton::setChevronGlyph(const QString& glyph)
{
    if (m_chevronGlyph == glyph)
        return;
    m_chevronGlyph = glyph;
    update();
    emit chevronChanged();
}

void DropDownButton::setIconFontFamily(const QString& family)
{
    if (m_iconFontFamily == family)
        return;
    m_iconFontFamily = family;
    update();
    emit chevronChanged();
}

void DropDownButton::setChevronSize(int size)
{
    if (m_chevronSize == size)
        return;
    m_chevronSize = size;
    updateGeometry();
    update();
    emit chevronChanged();
}

void DropDownButton::setChevronOffset(const QPoint& offset)
{
    if (m_chevronOffset == offset)
        return;
    m_chevronOffset = offset;
    updateGeometry();
    update();
    emit chevronChanged();
}

void DropDownButton::setPressProgress(qreal value)
{
    qreal clamped = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_pressProgress, clamped))
        return;
    m_pressProgress = clamped;
    update();
}

QSize DropDownButton::sizeHint() const
{
    QSize size = Button::sizeHint();
    size.rwidth() += chevronReserveWidth();
    return size;
}

QSize DropDownButton::minimumSizeHint() const
{
    return sizeHint();
}

QRectF DropDownButton::contentPaintRect(const QRectF& surfaceRect) const
{
    const qreal reserve = qMin(surfaceRect.width(), static_cast<qreal>(chevronReserveWidth()));
    if (layoutDirection() == Qt::RightToLeft)
        return surfaceRect.adjusted(reserve, 0, 0, 0);
    return surfaceRect.adjusted(0, 0, -reserve, 0);
}

int DropDownButton::chevronReserveWidth() const
{
    const auto& spacing = themeSpacing();
    const int gap = fluentSize() == Small ? spacing.gap.tight : spacing.gap.normal;
    return gap + qMax(0, m_chevronSize) + qMax(0, m_chevronOffset.x());
}

void DropDownButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_pressAnimation) {
            m_pressAnimation->stop();
            m_pressAnimation->setStartValue(0.0);
            m_pressAnimation->setEndValue(1.0);
            ::fluent::detail::startMotionTransition(m_pressAnimation, themeAnimation().slow);
        }

        if (m_menu) {
            // Open asynchronously so callers are not blocked by a nested exec() loop.
            event->accept();
            detail::showMenuButtonMenu(this);
            return;
        }
    }
    Button::mousePressEvent(event);
}

void DropDownButton::keyPressEvent(QKeyEvent* event)
{
    const bool altDown = event->key() == Qt::Key_Down && event->modifiers().testFlag(Qt::AltModifier);
    const bool f4 = event->key() == Qt::Key_F4 && event->modifiers() == Qt::NoModifier;
    const bool primaryOpen =
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() == Qt::NoModifier;
    if (m_menu && (altDown || f4 || primaryOpen)) {
        detail::showMenuButtonMenu(this);
        event->accept();
        return;
    }
    Button::keyPressEvent(event);
}

void DropDownButton::paintEvent(QPaintEvent*)
{
    // Paint open-state visuals without mutating the interactionState property.
    paintButton(m_isOpen ? Pressed : interactionState());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const bool usesFluentIcons = m_iconFontFamily == Typography::FontFamily::FluentIcons;
    QFont iconFont = usesFluentIcons ? Typography::Icons::font(m_chevronSize) : QFont(m_iconFontFamily);
    if (!usesFluentIcons)
        iconFont.setPixelSize(m_chevronSize);
    painter.setFont(iconFont);
    const qreal pressEffect = qSin(m_pressProgress * M_PI);

    const auto& colors = themeColorsRef();
    QColor textColor;
    if (!isEnabled()) {
        textColor = colors.textDisabled;
    } else {
        textColor = (fluentStyle() == Accent) ? colors.textOnAccent : colors.textPrimary;
        if (pressEffect > 0.0) {
            // Multiply existing alpha rather than resetting to 255 so secondary tokens keep their opacity.
            const qreal alphaFactor = 1.0 - 0.5 * pressEffect;
            textColor.setAlphaF(textColor.alphaF() * alphaFactor);
        }
    }

    painter.setPen(textColor);

    // Snap the Y press dip to whole pixels so the compact 12 px chevron stays sharp.
    const qreal maxOffset = 3.0;
    const qreal pressOffset = qRound(maxOffset * pressEffect);
    const QRectF bounds = QRectF(rect());
    const qreal chevronX = layoutDirection() == Qt::RightToLeft
        ? bounds.left() + m_chevronOffset.x()
        : bounds.right() - m_chevronOffset.x() - m_chevronSize;
    QRectF chevronSlot(chevronX, bounds.center().y() - m_chevronSize * 0.5, m_chevronSize, m_chevronSize);
    chevronSlot.translate(0, pressOffset + m_chevronOffset.y());
    if (usesFluentIcons) {
        Typography::Icons::paintGlyph(painter, chevronSlot, m_chevronGlyph, m_chevronSize, Qt::AlignCenter);
    } else {
        painter.drawText(chevronSlot, Qt::AlignCenter, m_chevronGlyph);
    }
}

} // namespace fluent::basicinput
