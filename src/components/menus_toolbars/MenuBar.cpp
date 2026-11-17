#include "compatibility/private/WidgetPlatformCompat_p.h"
#include "MenuBar.h"
#include "Menu.h"
#include "components/basicinput/DropDownButton.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"

#include <QAction>
#include <QActionEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QStyle>

namespace fluent::menus_toolbars {

namespace {
QString stripAccessMarkers(QString text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    if (tabIndex >= 0)
        text.truncate(tabIndex);

    QString result;
    result.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('&')) {
                result.append(QLatin1Char('&'));
                ++i;
            }
            continue;
        }
        result.append(text.at(i));
    }
    return result;
}

QString accessMarkerFromText(const QString& text)
{
    for (int i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) != QLatin1Char('&'))
            continue;
        if (text.at(i + 1) == QLatin1Char('&')) {
            ++i;
            continue;
        }
        return text.mid(i + 1, 1).toUpper();
    }
    return QString();
}

QString displayText(QAction* action)
{
    return action ? stripAccessMarkers(action->text()) : QString();
}

QString accessKeyText(QAction* action)
{
    if (!action)
        return QString();
    const QVariant prop = action->property("accessKey");
    if (prop.isValid() && !prop.toString().trimmed().isEmpty())
        return prop.toString().trimmed().left(1).toUpper();
    return accessMarkerFromText(action->text());
}
} // namespace

FluentMenuBar::FluentMenuBar(QWidget* parent) : QMenuBar(parent)
{
    setAttribute(Qt::WA_Hover);
    ::compatibility::detail::disableNativeFocusRing(this);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    ::compatibility::detail::useWidgetMenuBar(this);
    m_overflowButton = new basicinput::DropDownButton(this);
    m_overflowButton->setObjectName(QStringLiteral("FluentMenuBar.MoreButton"));
    m_overflowButton->setAccessibleName(tr("More menus"));
    m_overflowButton->setToolTip(tr("More menus"));
    m_overflowButton->setFluentStyle(basicinput::Button::Subtle);
    m_overflowButton->setFocusVisual(true);
    m_overflowButton->setChevronGlyph(Typography::Icons::More);
    m_overflowButton->hide();
    onThemeUpdated();
    ::compatibility::detail::hideMenuBarExtensionButton(this);
}

void FluentMenuBar::setFontStyle(Typography::FontRole role)
{
    if (m_fontStyle == role)
        return;
    m_fontStyle = role;
    onThemeUpdated();
    emit fontStyleChanged();
}

QRect FluentMenuBar::fluentActionGeometry(QAction* action) const
{
    ensureLayout();
    return m_actionRects.value(action);
}

void FluentMenuBar::setBackgroundVisible(bool visible)
{
    if (m_backgroundVisible == visible)
        return;
    m_backgroundVisible = visible;
    update();
    emit backgroundVisibleChanged(visible);
}

void FluentMenuBar::onThemeUpdated()
{
    const QPointer<FluentMenuBar> guard(this);
    setFont(themeFont(m_fontStyle).toQFont());
    ::compatibility::detail::hideMenuBarExtensionButton(this);
    invalidateLayout();
    syncOverflow();
    if (!guard)
        return;
    updateGeometry();
    update();
}

void FluentMenuBar::paintEvent(QPaintEvent*)
{
    ensureLayout();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();
    const Metrics currentMetrics = metrics();
    if (m_backgroundVisible)
        painter.fillRect(rect(), colors.bgCanvas);
    painter.setFont(font());

    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;

        const QRect itemRect = m_actionRects.value(action);
        if (itemRect.isEmpty() || !rect().intersects(itemRect))
            continue;

        const bool isEnabled = action->isEnabled();
        const bool isPressed = (action == m_pressedAction);
        const bool isOpen = (action == m_openAction);
        const bool isHovered = (action == m_hoveredAction);
        QColor fill = Qt::transparent;
        if (isEnabled) {
            if (isPressed)
                fill = colors.subtleTertiary;
            else if (isOpen || isHovered)
                fill = colors.subtleSecondary;
        }

        if (fill.isValid() && fill.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(itemRect, currentMetrics.cornerRadius, currentMetrics.cornerRadius);
        }

        const QColor textColor = isEnabled ? colors.textPrimary : colors.textDisabled;
        painter.setPen(textColor);
        const QRect textRect = itemRect.adjusted(currentMetrics.horizontalPadding, 0, -currentMetrics.horizontalPadding,
            0);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, displayText(action));
    }
}

bool FluentMenuBar::event(QEvent* event)
{
    const QPointer<FluentMenuBar> guard(this);
    const bool handled = QMenuBar::event(event);
    if (!guard)
        return handled;
    if (event
        && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show || event->type() == QEvent::Resize
            || event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange
            || event->type() == QEvent::LayoutDirectionChange || event->type() == QEvent::ActionAdded
            || event->type() == QEvent::ActionRemoved || event->type() == QEvent::ActionChanged
            || event->type() == QEvent::ContentsRectChange)) {
        ::compatibility::detail::hideMenuBarExtensionButton(this);
        invalidateLayout();
        syncOverflow();
    }
    return handled;
}

void FluentMenuBar::resizeEvent(QResizeEvent* event)
{
    QMenuBar::resizeEvent(event);
    ::compatibility::detail::hideMenuBarExtensionButton(this);
    invalidateLayout();
    syncOverflow();
}

void FluentMenuBar::actionEvent(QActionEvent* event)
{
    const QPointer<FluentMenuBar> guard(this);
    if (event->type() == QEvent::ActionAdded && event->action()) {
        QAction* added = event->action();
        if (added->menu())
            added->menu()->installEventFilter(this);
        QObject::disconnect(m_actionChangedConnections.take(added));
        m_actionChangedConnections.insert(added, connect(added, &QAction::changed, this, [this, added]() {
            const QPointer<FluentMenuBar> lifetime(this);
            if (added->menu())
                added->menu()->installEventFilter(this);
            invalidateLayout();
            syncOverflow();
            if (!lifetime)
                return;
            updateGeometry();
            update();
        }));
    } else if (event->type() == QEvent::ActionRemoved && event->action()) {
        QAction* removed = event->action();
        QObject::disconnect(m_actionChangedConnections.take(removed));
        if (removed->menu())
            removed->menu()->removeEventFilter(this);
        m_actionRects.remove(removed);
        if (m_hoveredAction == removed)
            m_hoveredAction = nullptr;
        if (m_pressedAction == removed)
            m_pressedAction = nullptr;
        if (m_focusedAction == removed)
            m_focusedAction = nullptr;
        if (m_openAction == removed)
            m_openAction = nullptr;
    }

    QMenuBar::actionEvent(event);
    if (!guard)
        return;
    ::compatibility::detail::hideMenuBarExtensionButton(this);
    invalidateLayout();
    syncOverflow();
    if (!guard)
        return;
    updateGeometry();
    update();
}

void FluentMenuBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QMenuBar::mouseMoveEvent(event);
        return;
    }

    QAction* action = actionAt(event->pos(), false);
    setHoveredAction(action);

    if (m_openAction && action && action != m_openAction && action->isEnabled() && action->menu()) {
        setFocusedAction(action);
        openMenuForAction(action);
    }

    event->accept();
}

void FluentMenuBar::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QMenuBar::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);
    QAction* action = actionAt(event->pos(), true);
    if (!action) {
        QMenuBar::mousePressEvent(event);
        return;
    }

    m_pressedAction = action;
    setHoveredAction(action);
    setFocusedAction(action);
    update();
    event->accept();
}

void FluentMenuBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QMenuBar::mouseReleaseEvent(event);
        return;
    }

    QAction* pressed = m_pressedAction;
    QAction* released = actionAt(event->pos(), true);
    clearPressedAction();
    if (pressed && pressed == released) {
        activateAction(pressed);
        event->accept();
        return;
    }

    event->accept();
}

void FluentMenuBar::leaveEvent(QEvent* event)
{
    QMenuBar::leaveEvent(event);
    if (!m_openAction)
        setHoveredAction(nullptr);
}

void FluentMenuBar::focusInEvent(QFocusEvent* event)
{
    QMenuBar::focusInEvent(event);
    if (!m_focusedAction)
        m_focusedAction = firstEnabledAction();
    update();
}

void FluentMenuBar::focusOutEvent(QFocusEvent* event)
{
    QMenuBar::focusOutEvent(event);
    clearPressedAction();
    update();
}

void FluentMenuBar::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled()) {
        QMenuBar::keyPressEvent(event);
        return;
    }

    if (event->modifiers().testFlag(Qt::AltModifier)) {
        QString key = event->text().left(1).toUpper();
        if (key.isEmpty()
            && ((event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z)
                || (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9))) {
            // Windows system-key events can omit text() while keeping the Latin key code.
            key = QString(QChar(static_cast<ushort>(event->key()))).toUpper();
        }
        if (!key.isEmpty()) {
            for (QAction* action : actions()) {
                if (!action || !action->isVisible() || !action->isEnabled())
                    continue;
                if (accessKeyText(action) != key)
                    continue;
                setFocusedAction(action);
                activateAction(action);
                event->accept();
                return;
            }
        }
    }

    switch (event->key()) {
    case Qt::Key_Left:
        setFocusedAction(nextEnabledAction(m_focusedAction, layoutDirection() == Qt::RightToLeft ? 1 : -1));
        event->accept();
        return;
    case Qt::Key_Right:
        setFocusedAction(nextEnabledAction(m_focusedAction, layoutDirection() == Qt::RightToLeft ? -1 : 1));
        event->accept();
        return;
    case Qt::Key_Home:
        setFocusedAction(firstEnabledAction());
        event->accept();
        return;
    case Qt::Key_End:
        setFocusedAction(nextEnabledAction(firstEnabledAction(), -1));
        event->accept();
        return;
    case Qt::Key_Down:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (m_focusedAction) {
            activateAction(m_focusedAction);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Escape:
        closeOpenMenu();
        event->accept();
        return;
    default:
        break;
    }

    QMenuBar::keyPressEvent(event);
}

bool FluentMenuBar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        for (QAction* action : actions()) {
            if (!action || action->menu() != watched)
                continue;
            // QMenuBar may route native mnemonics directly to the QMenu; mirror its open state here.
            m_openAction = action;
            setFocusedAction(action);
            setHoveredAction(action);
            update();
            break;
        }
    }
    if ((event->type() == QEvent::Hide || event->type() == QEvent::Close) && m_openAction
        && watched == m_openAction->menu()) {
        m_openAction = nullptr;
        m_hoveredAction = nullptr;
        m_pressedAction = nullptr;
        update();
    }
    return false;
}

QSize FluentMenuBar::sizeHint() const
{
    ensureLayout();
    return QSize(m_naturalWidth, metrics().rowHeight);
}

FluentMenuBar::Metrics FluentMenuBar::metrics() const
{
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();
    const int itemExtent = qMax(spacing.controlHeight.standard, QFontMetrics(font()).height() + spacing.xSmall);
    Metrics result;
    result.rowHeight = itemExtent;
    result.itemHeight = itemExtent;
    result.horizontalPadding = spacing.small;
    result.itemSpacing = spacing.gap.tight;
    result.cornerRadius = radius.control;
    return result;
}

void FluentMenuBar::invalidateLayout()
{
    m_layoutDirty = true;
}

void FluentMenuBar::ensureLayout() const
{
    if (!m_layoutDirty)
        return;

    m_actionRects.clear();
    m_overflowActions.clear();
    m_overflowRect = QRect();

    const Metrics currentMetrics = metrics();
    const QFontMetrics fontMetrics(font());
    const QRect content = contentsRect();
    int x = content.left();
    const int y = contentsRect().top() + (qMax(currentMetrics.rowHeight, height()) - currentMetrics.itemHeight) / 2;

    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;

        const int textWidth = fontMetrics.horizontalAdvance(displayText(action));
        const int itemWidth = qMax(currentMetrics.itemHeight, textWidth + currentMetrics.horizontalPadding * 2);
        m_actionRects.insert(action, QRect(x, y, itemWidth, currentMetrics.itemHeight));
        x += itemWidth + currentMetrics.itemSpacing;
    }
    m_naturalWidth = qMax(0, x - currentMetrics.itemSpacing - content.left());
    const bool overflow = m_naturalWidth > content.width();
    const int overflowWidth = overflow ? qMin(currentMetrics.itemHeight, qMax(0, content.width())) : 0;
    const int availableRight = content.right() + 1 - overflowWidth;
    if (overflow)
        m_overflowRect = QStyle::visualRect(layoutDirection(), content,
            QRect(availableRight, y, overflowWidth, currentMetrics.itemHeight));
    for (QAction* action : actions()) {
        if (!m_actionRects.contains(action))
            continue;
        const QRect natural = m_actionRects.value(action);
        if (overflow && natural.right() + 1 > availableRight) {
            m_overflowActions.append(action);
            m_actionRects.remove(action);
        } else {
            m_actionRects[action] = QStyle::visualRect(layoutDirection(), content, natural);
        }
    }
    m_layoutDirty = false;
}

void FluentMenuBar::syncOverflow()
{
    if (!m_overflowButton)
        return;
    ensureLayout();
    if (!m_overflowMenu && m_overflowActions.isEmpty()) {
        m_overflowButton->hide();
        return;
    }
    if (!m_overflowMenu) {
        m_overflowMenu = new FluentMenu(tr("More menus"), this);
        m_overflowMenu->setObjectName(QStringLiteral("FluentMenuBar.OverflowMenu"));
        m_overflowButton->setMenu(m_overflowMenu);
    }
    // Reuse the original actions directly so enabled/checked state, submenus, and slots stay synchronized.
    if (m_overflowMenu->actions() != m_overflowActions) {
        const QPointer<FluentMenuBar> guard(this);
        m_overflowMenu->hide();
        if (!guard)
            return;
        for (QAction* action : m_overflowMenu->actions())
            m_overflowMenu->removeAction(action);
        for (QAction* action : m_overflowActions)
            m_overflowMenu->addAction(action);
    }
    m_overflowMenu->setLayoutDirection(layoutDirection());
    m_overflowButton->setGeometry(m_overflowRect);
    m_overflowButton->setVisible(!m_overflowActions.isEmpty());
    m_overflowButton->raise();
}

QAction* FluentMenuBar::actionAt(const QPoint& position, bool enabledOnly) const
{
    ensureLayout();
    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;
        if (enabledOnly && !action->isEnabled())
            continue;
        if (m_actionRects.value(action).contains(position))
            return action;
    }
    return nullptr;
}

QAction* FluentMenuBar::firstEnabledAction() const
{
    for (QAction* action : actions()) {
        if (action && action->isVisible() && action->isEnabled())
            return action;
    }
    return nullptr;
}

QAction* FluentMenuBar::nextEnabledAction(QAction* from, int direction) const
{
    QVector<QAction*> enabledActions;
    for (QAction* action : actions()) {
        if (action && action->isVisible() && action->isEnabled())
            enabledActions.append(action);
    }
    if (enabledActions.isEmpty())
        return nullptr;

    int index = enabledActions.indexOf(from);
    if (index < 0)
        index = direction < 0 ? 0 : -1;
    index = (index + direction + enabledActions.size()) % enabledActions.size();
    return enabledActions.at(index);
}

void FluentMenuBar::setHoveredAction(QAction* action)
{
    if (m_hoveredAction == action)
        return;
    m_hoveredAction = action;
    update();
}

void FluentMenuBar::setFocusedAction(QAction* action)
{
    if (!action || !action->isVisible() || !action->isEnabled())
        action = firstEnabledAction();
    if (m_focusedAction == action)
        return;
    m_focusedAction = action;
    update();
}

void FluentMenuBar::clearPressedAction()
{
    if (!m_pressedAction)
        return;
    m_pressedAction = nullptr;
    update();
}

void FluentMenuBar::activateAction(QAction* action)
{
    if (!action || !action->isVisible() || !action->isEnabled())
        return;

    setFocusedAction(action);
    if (action->menu()) {
        openMenuForAction(action);
        return;
    }

    const QPointer<FluentMenuBar> guard(this);
    const QPointer<QAction> actionGuard(action);
    closeOpenMenu();
    if (guard && actionGuard)
        actionGuard->trigger();
}

void FluentMenuBar::openMenuForAction(QAction* action)
{
    if (!action || !action->menu() || !action->isVisible() || !action->isEnabled())
        return;

    const QPointer<FluentMenuBar> guard(this);
    const QPointer<QAction> actionGuard(action);
    if (m_openAction && m_openAction != action && m_openAction->menu())
        m_openAction->menu()->hide();
    if (!guard || !actionGuard || !actionGuard->menu())
        return;

    ensureLayout();
    const QRect itemRect = m_actionRects.value(action, m_overflowRect);
    if (itemRect.isEmpty())
        return;

    QMenu* menu = action->menu();
    menu->installEventFilter(this);
    m_openAction = action;
    setHoveredAction(action);
    update();
    ::fluent::overlay::popupMenuAt(menu, this, QPoint(itemRect.left(), itemRect.bottom() + 1));
}

void FluentMenuBar::closeOpenMenu()
{
    const QPointer<QMenu> menu = m_openAction ? m_openAction->menu() : nullptr;
    m_openAction = nullptr;
    m_hoveredAction = nullptr;
    m_pressedAction = nullptr;
    update();
    // aboutToHide may destroy the bar, so clear local state before hiding the menu.
    if (menu)
        menu->hide();
}

} // namespace fluent::menus_toolbars
