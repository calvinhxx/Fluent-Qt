#include "compatibility/PaintCompat.h"
#include "CommandPresenter_p.h"

#include <QAction>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeySequence>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QVariant>
#include <QWindow>

#include <utility>

#include "components/menus_toolbars/private/CommandAccessibility_p.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::menus_toolbars::detail {
namespace {

QString stripAccessMarkers(QString text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    if (tabIndex >= 0)
        text.truncate(tabIndex);

    QString result;
    result.reserve(text.size());
    for (int index = 0; index < text.size(); ++index) {
        if (text.at(index) != QLatin1Char('&')) {
            result.append(text.at(index));
            continue;
        }
        if (index + 1 < text.size() && text.at(index + 1) == QLatin1Char('&')) {
            result.append(QLatin1Char('&'));
            ++index;
        }
    }
    return result.trimmed();
}

QString primaryCaption(const QAction* action)
{
    if (!action)
        return {};
    const QString iconText = stripAccessMarkers(action->iconText());
    return iconText.isEmpty() ? stripAccessMarkers(action->text()) : iconText;
}

QString overflowCaption(const QAction* action)
{
    if (!action)
        return {};
    const QString text = stripAccessMarkers(action->text());
    return text.isEmpty() ? stripAccessMarkers(action->iconText()) : text;
}

QString embeddedShortcutText(const QString& text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    return tabIndex < 0 ? QString() : text.mid(tabIndex + 1).trimmed();
}

QString shortcutText(const QAction* action)
{
    if (!action)
        return {};
    const QString native = action->shortcut().toString(QKeySequence::NativeText);
    return native.isEmpty() ? embeddedShortcutText(action->text()) : native;
}

QRect visualRect(Qt::LayoutDirection direction, const QRect& bounds, const QRect& logical)
{
    return QStyle::visualRect(direction, bounds, logical);
}

} // namespace

void updateCommandFocusVisual(basicinput::Button* button, const QEvent* event)
{
    if (!button || !event)
        return;
    if (event->type() == QEvent::MouseButtonPress) {
        button->setFocusVisual(false);
    } else if (event->type() == QEvent::KeyPress) {
        button->setFocusVisual(true);
    } else if (event->type() == QEvent::FocusIn) {
        const auto reason = static_cast<const QFocusEvent*>(event)->reason();
        if (reason == Qt::MouseFocusReason)
            button->setFocusVisual(false);
        else if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason || reason == Qt::ShortcutFocusReason
            || reason == Qt::OtherFocusReason)
            button->setFocusVisual(true);
        // Popup and window activation restore the existing input modality.
    }
}

CommandPresenter::CommandPresenter(QAction* action, Mode mode, ActivationHandler activationHandler, QWidget* parent)
    : basicinput::Button(parent), m_action(action), m_mode(mode), m_activationHandler(std::move(activationHandler))
{
    setFluentStyle(basicinput::Button::Subtle);
    setFocusVisual(false);
    // Private presenters stay out of the Tab chain, but must accept explicit
    // composite-navigation and pointer focus. Qt::NoFocus can redirect an
    // explicit setFocus() back to the editor on some platform plugins.
    // zh_CN: 私有 presenter 不进入 Tab 链，但必须接收组合导航和指针焦点；
    // 部分平台插件会把对 Qt::NoFocus 控件的显式 setFocus() 重定向回编辑器。
    setFocusPolicy(Qt::ClickFocus);
    setMinimumHeight(::Spacing::ControlHeight::Large);
    setAutoDefault(false);
    setDefault(false);
    markCommandAccessibleWidget(this,
        m_mode == Mode::Primary ? CommandAccessibleRole::PrimaryCommand : CommandAccessibleRole::MenuCommand);

    if (action) {
        connect(action, &QAction::changed, this, [this]() { synchronize(); });
        connect(action, &QObject::destroyed, this, [this]() {
            m_action.clear();
            setProperty("commandAction", QVariant());
        });
    }

    connect(this, &QPushButton::clicked, this, [this]() {
        const QPointer<QAction> actionGuard = m_action;
        const ActivationHandler handler = m_activationHandler;
        if (actionGuard && handler)
            handler(this, actionGuard.data());
    });

    synchronize();
}

void CommandPresenter::setPrimaryLabelCollapsed(bool collapsed)
{
    if (m_primaryLabelCollapsed == collapsed)
        return;
    m_primaryLabelCollapsed = collapsed;
    synchronize();
}

void CommandPresenter::synchronize()
{
    QAction* command = m_action.data();
    if (!command)
        return;

    if (m_menu != command->menu()) {
        disconnect(m_menuDestroyed);
        m_menu = command->menu();
        m_menuOpen = false;
        if (m_menu) {
            m_menuDestroyed = connect(m_menu, &QObject::destroyed, this, [this] {
                m_menu.clear();
                m_menuOpen = false;
                synchronize();
            });
        }
    }
    updateCommandExpandedAccessibility(this, m_menuOpen, hasMenu());

    m_displayText = m_mode == Mode::Primary ? primaryCaption(command) : overflowCaption(command);
    m_shortcutText = shortcutText(command);
    m_displayIcon = command->icon();

    setEnabled(command->isEnabled());
    setCheckable(command->isCheckable());
    setChecked(command->isChecked());
    setToolTip(command->toolTip().isEmpty()
            ? (m_mode == Mode::Primary && m_primaryLabelCollapsed && !m_displayIcon.isNull()
                      ? m_displayText
                      : QString())
            : command->toolTip());
    setStatusTip(command->statusTip());
    setAccessibleName(m_displayText);
    setAccessibleDescription(command->toolTip().isEmpty() ? command->statusTip() : command->toolTip());
    setProperty("commandAction", QVariant::fromValue(static_cast<QObject*>(command)));
    setProperty("commandText", m_displayText);
    setProperty("commandShortcut", m_shortcutText);

    if (m_mode == Mode::Primary) {
        const bool showText = !m_primaryLabelCollapsed || m_displayIcon.isNull();
        setText(showText ? m_displayText : QString());
        setIcon(m_displayIcon);
        setIconSize(QSize(20, 20));
        if (!m_displayIcon.isNull() && showText) {
            setFluentLayout(
                layoutDirection() == Qt::RightToLeft ? basicinput::Button::IconAfter : basicinput::Button::IconBefore);
        } else if (!m_displayIcon.isNull()) {
            setFluentLayout(basicinput::Button::IconOnly);
        } else {
            setFluentLayout(basicinput::Button::TextOnly);
        }
    } else {
        // Overflow content is painted with leading/trailing alignment below.
        basicinput::Button::setText(QString());
        basicinput::Button::setIcon(QIcon());
        setFluentLayout(basicinput::Button::TextOnly);
    }

    updateGeometry();
    update();
}

QSize CommandPresenter::sizeHint() const
{
    if (m_mode == Mode::Primary) {
        const QSize base = basicinput::Button::sizeHint();
        const int menuWidth = hasMenu() ? 16 + ::Spacing::Gap::Tight : 0;
        return QSize(qMax(::Spacing::ControlHeight::Large, base.width()) + menuWidth, ::Spacing::ControlHeight::Large);
    }

    const QFontMetrics metrics(font());
    const int checkSlot = 16;
    const int iconSlot = 16;
    const int shortcutWidth =
        m_shortcutText.isEmpty() ? 0 : metrics.horizontalAdvance(m_shortcutText) + ::Spacing::Gap::Normal;
    const int contentWidth = ::Spacing::Medium * 2 + checkSlot + ::Spacing::Gap::Tight + iconSlot
        + ::Spacing::Gap::Normal + metrics.horizontalAdvance(m_displayText) + shortcutWidth
        + (hasMenu() ? 16 + ::Spacing::Gap::Normal : 0);
    return QSize(qMax(180, contentWidth), ::Spacing::ControlHeight::Large);
}

QSize CommandPresenter::minimumSizeHint() const
{
    return m_mode == Mode::Primary
        ? QSize(::Spacing::ControlHeight::Large, ::Spacing::ControlHeight::Large)
        : QSize(120, ::Spacing::ControlHeight::Large);
}

void CommandPresenter::onThemeUpdated()
{
    setFont(themeFont(Typography::FontRole::Body).toQFont());
    basicinput::Button::onThemeUpdated();
    updateGeometry();
}

bool CommandPresenter::event(QEvent* event)
{
    updateCommandFocusVisual(this, event);
    return basicinput::Button::event(event);
}

void CommandPresenter::paintEvent(QPaintEvent* event)
{
    if (m_menuOpen && isEnabled())
        paintButton(Pressed);
    else
        basicinput::Button::paintEvent(event);
    if (m_mode == Mode::Overflow)
        paintOverflowContent();
    if (hasMenu()) {
        QPainter painter(this);
        painter.setPen(isEnabled() ? themeColorsRef().textPrimary : themeColorsRef().textDisabled);
        const int inset = m_mode == Mode::Overflow ? ::Spacing::Medium : ::Spacing::Small;
        const QRect logical(width() - inset - 16, (height() - 16) / 2, 16, 16);
        const auto glyph = m_mode == Mode::Primary
            ? Typography::Icons::ChevronDown
            : (layoutDirection() == Qt::RightToLeft
                      ? Typography::Icons::ChevronLeftMed
                      : Typography::Icons::ChevronRightMed);
        Typography::Icons::paintGlyph(painter, visualRect(layoutDirection(), rect(), logical), glyph, 12,
            Qt::AlignCenter);
    }
}

bool CommandPresenter::hasMenu() const
{
    return m_action && m_action->menu();
}

void CommandPresenter::setMenuOpen(bool open)
{
    if (m_menuOpen == open)
        return;
    m_menuOpen = open;
    updateCommandExpandedAccessibility(this, open, hasMenu());
    update();
}

QRectF CommandPresenter::contentPaintRect(const QRectF& surfaceRect) const
{
    const QRectF content = basicinput::Button::contentPaintRect(surfaceRect);
    if (m_mode != Mode::Primary || !hasMenu())
        return content;
    const qreal reserve = 16 + ::Spacing::Gap::Tight;
    return layoutDirection() == Qt::RightToLeft
        ? content.adjusted(reserve, 0, 0, 0)
        : content.adjusted(0, 0, -reserve, 0);
}

void CommandPresenter::nextCheckState()
{
    if (!hasMenu())
        basicinput::Button::nextCheckState();
}

void CommandPresenter::paintOverflowContent()
{
    QAction* command = m_action.data();
    if (!command)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setFont(font());

    const auto& colors = themeColorsRef();
    const QColor textColor = isEnabled() ? colors.textPrimary : colors.textDisabled;
    const QColor shortcutColor = isEnabled() ? colors.textSecondary : colors.textDisabled;

    const QRect bounds = rect();
    const QRect content = bounds.adjusted(::Spacing::Medium, 0, -::Spacing::Medium, 0);
    int logicalLeading = content.left();
    int logicalTrailing = content.right() + 1;
    if (hasMenu())
        logicalTrailing -= 16 + ::Spacing::Gap::Normal;

    const QRect checkLogical(logicalLeading, content.center().y() - 8, 16, 16);
    logicalLeading += 16 + ::Spacing::Gap::Tight;

    const QRect iconLogical(logicalLeading, content.center().y() - 8, 16, 16);
    logicalLeading += 16 + ::Spacing::Gap::Normal;

    const int shortcutWidth = m_shortcutText.isEmpty() ? 0 : painter.fontMetrics().horizontalAdvance(m_shortcutText);
    QRect shortcutLogical;
    if (shortcutWidth > 0) {
        shortcutLogical = QRect(logicalTrailing - shortcutWidth, content.top(), shortcutWidth, content.height());
        logicalTrailing -= shortcutWidth + ::Spacing::Gap::Normal;
    }

    const QRect textLogical(logicalLeading, content.top(), qMax(0, logicalTrailing - logicalLeading), content.height());

    if (command->isCheckable() && command->isChecked()) {
        painter.setPen(textColor);
        Typography::Icons::paintGlyph(painter, visualRect(layoutDirection(), bounds, checkLogical),
            Typography::Icons::CheckMark, 12, Qt::AlignCenter);
    }

    if (!m_displayIcon.isNull()) {
        const qreal targetDpr = painter.device()
            ? qMax<qreal>(1.0, painter.device()->devicePixelRatioF())
            : qMax<qreal>(1.0, devicePixelRatioF());
        QPixmap pixmap = fluentIconPixmapForLogicalExtent(m_displayIcon, QSize(16, 16), targetDpr,
            window() ? window()->windowHandle() : nullptr);
        if (!pixmap.isNull()) {
            const QRect target = visualRect(layoutDirection(), bounds, iconLogical);
            const QSize logicalSize = fluentPixmapLogicalSize(pixmap);
            const QPoint topLeft(target.center().x() - logicalSize.width() / 2,
                target.center().y() - logicalSize.height() / 2);
            painter.drawPixmap(topLeft, pixmap);
        }
    }

    const QRect textVisual = visualRect(layoutDirection(), bounds, textLogical);
    const QString elided = painter.fontMetrics().elidedText(m_displayText, Qt::ElideRight, textVisual.width());
    painter.setPen(textColor);
    painter.drawText(textVisual,
        QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter) | Qt::TextSingleLine, elided);

    if (!shortcutLogical.isEmpty()) {
        painter.setPen(shortcutColor);
        painter.drawText(visualRect(layoutDirection(), bounds, shortcutLogical), Qt::AlignCenter | Qt::TextSingleLine,
            m_shortcutText);
    }
}

CommandMoreButton::CommandMoreButton(QWidget* parent) : basicinput::Button(parent)
{
    markCommandAccessibleWidget(this, CommandAccessibleRole::MoreButton);
    setFluentStyle(basicinput::Button::Subtle);
    setFluentLayout(basicinput::Button::IconOnly);
    setIconGlyph(Typography::Icons::More, 20);
    setFocusVisual(false);
    setFocusPolicy(Qt::ClickFocus);
    setMinimumSize(::Spacing::ControlHeight::Large, ::Spacing::ControlHeight::Large);
    const QString moreCommands = QCoreApplication::translate("fluent::menus_toolbars::CommandBar", "More commands");
    setToolTip(moreCommands);
    setAccessibleName(moreCommands);
    setProperty("commandText", moreCommands);
    setExpandedState(false, false);
}

void CommandMoreButton::setExpandedState(bool expanded, bool expandable)
{
    updateCommandExpandedAccessibility(this, expanded, expandable);
}

bool CommandMoreButton::event(QEvent* event)
{
    updateCommandFocusVisual(this, event);
    return basicinput::Button::event(event);
}

} // namespace fluent::menus_toolbars::detail
