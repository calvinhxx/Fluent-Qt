#include "MenuBar.h"

#include <QAction>
#include <QActionEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

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

FluentMenuBar::FluentMenuBar(QWidget* parent)
    : QMenuBar(parent)
{
    setAttribute(Qt::WA_Hover);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
#ifdef Q_OS_MAC
    setNativeMenuBar(false);
#endif
    onThemeUpdated();
}

void FluentMenuBar::setFontStyle(const QString& style)
{
    const QString normalized = style.trimmed().isEmpty() ? QStringLiteral("Body") : style.trimmed();
    if (m_fontStyle == normalized)
        return;
    m_fontStyle = normalized;
    onThemeUpdated();
    emit fontStyleChanged();
}

QRect FluentMenuBar::fluentActionGeometry(QAction* action) const
{
    ensureLayout();
    return m_actionRects.value(action);
}

void FluentMenuBar::onThemeUpdated()
{
    setFont(themeFont(m_fontStyle).toQFont());
    invalidateLayout();
    updateGeometry();
    update();
}

void FluentMenuBar::paintEvent(QPaintEvent*)
{
    ensureLayout();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColors();
    const Metrics currentMetrics = metrics();
    painter.fillRect(rect(), colors.bgCanvas);
    painter.setFont(font());

    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;

        const QRect itemRect = m_actionRects.value(action);
        if (itemRect.isEmpty() || !rect().intersects(itemRect))
            continue;

        QColor fill = Qt::transparent;
        if (action->isEnabled()) {
            if (action == m_pressedAction)
                fill = colors.subtleTertiary;
            else if (action == m_openAction || action == m_hoveredAction)
                fill = colors.subtleSecondary;
        }

        if (fill.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(itemRect, currentMetrics.cornerRadius, currentMetrics.cornerRadius);
        }

        painter.setPen(action->isEnabled() ? colors.textPrimary : colors.textDisabled);
        const QRect textRect = itemRect.adjusted(currentMetrics.horizontalPadding, 0, -currentMetrics.horizontalPadding, 0);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, displayText(action));
    }
}

void FluentMenuBar::resizeEvent(QResizeEvent* event)
{
    QMenuBar::resizeEvent(event);
    invalidateLayout();
}

void FluentMenuBar::actionEvent(QActionEvent* event)
{
    if (event->type() == QEvent::ActionAdded && event->action()) {
        connect(event->action(), &QAction::changed, this, [this]() {
            invalidateLayout();
            updateGeometry();
            update();
        });
    } else if (event->type() == QEvent::ActionRemoved && event->action()) {
        QAction* removed = event->action();
        m_actionRects.remove(removed);
        if (m_hoveredAction == removed) m_hoveredAction = nullptr;
        if (m_pressedAction == removed) m_pressedAction = nullptr;
        if (m_focusedAction == removed) m_focusedAction = nullptr;
        if (m_openAction == removed) m_openAction = nullptr;
    }

    QMenuBar::actionEvent(event);
    invalidateLayout();
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

    if (event->modifiers().testFlag(Qt::AltModifier) && !event->text().isEmpty()) {
        const QString key = event->text().left(1).toUpper();
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

    switch (event->key()) {
    case Qt::Key_Left:
        setFocusedAction(nextEnabledAction(m_focusedAction, -1));
        event->accept();
        return;
    case Qt::Key_Right:
        setFocusedAction(nextEnabledAction(m_focusedAction, 1));
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
    if ((event->type() == QEvent::Hide || event->type() == QEvent::Close) && m_openAction && watched == m_openAction->menu()) {
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
    const Metrics currentMetrics = metrics();
    int widthValue = 0;
    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;
        widthValue = qMax(widthValue, m_actionRects.value(action).right() + 1);
    }
    return QSize(widthValue, currentMetrics.rowHeight);
}

FluentMenuBar::Metrics FluentMenuBar::metrics() const
{
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();
    Metrics result;
    result.rowHeight = spacing.controlHeight.standard;
    result.itemHeight = spacing.controlHeight.standard;
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

    const Metrics currentMetrics = metrics();
    const QFontMetrics fontMetrics(font());
    int x = contentsRect().left();
    const int y = contentsRect().top() + (qMax(currentMetrics.rowHeight, height()) - currentMetrics.itemHeight) / 2;

    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;

        const int textWidth = fontMetrics.horizontalAdvance(displayText(action));
        const int itemWidth = qMax(currentMetrics.itemHeight, textWidth + currentMetrics.horizontalPadding * 2);
        m_actionRects.insert(action, QRect(x, y, itemWidth, currentMetrics.itemHeight));
        x += itemWidth + currentMetrics.itemSpacing;
    }

    m_layoutDirty = false;
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

    closeOpenMenu();
    action->trigger();
}

void FluentMenuBar::openMenuForAction(QAction* action)
{
    if (!action || !action->menu() || !action->isVisible() || !action->isEnabled())
        return;

    if (m_openAction && m_openAction != action && m_openAction->menu())
        m_openAction->menu()->hide();

    ensureLayout();
    const QRect itemRect = m_actionRects.value(action);
    if (itemRect.isEmpty())
        return;

    QMenu* menu = action->menu();
    menu->installEventFilter(this);
    m_openAction = action;
    setHoveredAction(action);
    update();
    menu->popup(mapToGlobal(QPoint(itemRect.left(), itemRect.bottom() + 1)));
}

void FluentMenuBar::closeOpenMenu()
{
    if (m_openAction && m_openAction->menu())
        m_openAction->menu()->hide();
    m_openAction = nullptr;
    m_hoveredAction = nullptr;
    m_pressedAction = nullptr;
    update();
}

} // namespace fluent::menus_toolbars
