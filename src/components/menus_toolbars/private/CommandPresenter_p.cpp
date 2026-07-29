#include "CommandPresenter_p.h"

#include <QAction>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QKeySequence>
#include <QPainter>
#include <QStyle>
#include <QVariant>
#include <QWindow>

#include <utility>

#include "compatibility/QtCompat.h"
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
        if (index + 1 < text.size()
            && text.at(index + 1) == QLatin1Char('&')) {
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
    return iconText.isEmpty() ? stripAccessMarkers(action->text())
                              : iconText;
}

QString overflowCaption(const QAction* action)
{
    if (!action)
        return {};
    const QString text = stripAccessMarkers(action->text());
    return text.isEmpty() ? stripAccessMarkers(action->iconText())
                          : text;
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
    const QString native =
        action->shortcut().toString(QKeySequence::NativeText);
    return native.isEmpty() ? embeddedShortcutText(action->text())
                            : native;
}

QRect visualRect(Qt::LayoutDirection direction,
                 const QRect& bounds,
                 const QRect& logical)
{
    return QStyle::visualRect(direction, bounds, logical);
}

} // namespace

CommandPresenter::CommandPresenter(
    QAction* action,
    Mode mode,
    ActivationHandler activationHandler,
    QWidget* parent)
    : basicinput::Button(parent),
      m_action(action),
      m_mode(mode),
      m_activationHandler(std::move(activationHandler))
{
    setFluentStyle(basicinput::Button::Subtle);
    setFocusVisual(true);
    // Private presenters stay out of the Tab chain, but must accept explicit
    // composite-navigation and pointer focus. Qt::NoFocus can redirect an
    // explicit setFocus() back to the editor on some platform plugins.
    // zh_CN: 私有 presenter 不进入 Tab 链，但必须接收组合导航和指针焦点；
    // 部分平台插件会把对 Qt::NoFocus 控件的显式 setFocus() 重定向回编辑器。
    setFocusPolicy(Qt::ClickFocus);
    setMinimumHeight(::Spacing::ControlHeight::Large);
    setAutoDefault(false);
    setDefault(false);
    markCommandAccessibleWidget(
        this,
        m_mode == Mode::Primary
            ? CommandAccessibleRole::PrimaryCommand
            : CommandAccessibleRole::MenuCommand);

    if (action) {
        connect(action,
                &QAction::changed,
                this,
                [this]() { synchronize(); });
        connect(action,
                &QObject::destroyed,
                this,
                [this]() {
                    m_action.clear();
                    setProperty("commandAction", QVariant());
                    setEnabled(false);
                    hide();
                });
    }

    connect(this,
            &QPushButton::clicked,
            this,
            [this]() {
                const QPointer<QAction> actionGuard = m_action;
                const ActivationHandler handler = m_activationHandler;
                if (actionGuard && handler)
                    handler(actionGuard.data());
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

    m_displayText = m_mode == Mode::Primary
        ? primaryCaption(command)
        : overflowCaption(command);
    m_shortcutText = shortcutText(command);
    m_displayIcon = command->icon();

    setEnabled(command->isEnabled());
    setCheckable(command->isCheckable());
    setChecked(command->isChecked());
    setToolTip(command->toolTip().isEmpty()
                   ? (m_mode == Mode::Primary
                          && m_primaryLabelCollapsed
                          && !m_displayIcon.isNull()
                       ? m_displayText
                       : QString())
                   : command->toolTip());
    setStatusTip(command->statusTip());
    setAccessibleName(m_displayText);
    setAccessibleDescription(
        command->toolTip().isEmpty()
            ? command->statusTip()
            : command->toolTip());
    setProperty(
        "commandAction",
        QVariant::fromValue(static_cast<QObject*>(command)));
    setProperty("commandText", m_displayText);
    setProperty("commandShortcut", m_shortcutText);

    if (m_mode == Mode::Primary) {
        const bool showText =
            !m_primaryLabelCollapsed || m_displayIcon.isNull();
        setText(showText ? m_displayText : QString());
        setIcon(m_displayIcon);
        setIconSize(QSize(20, 20));
        if (!m_displayIcon.isNull() && showText) {
            setFluentLayout(
                layoutDirection() == Qt::RightToLeft
                    ? basicinput::Button::IconAfter
                    : basicinput::Button::IconBefore);
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
        return QSize(qMax(::Spacing::ControlHeight::Large, base.width()),
                     ::Spacing::ControlHeight::Large);
    }

    const QFontMetrics metrics(font());
    const int checkSlot = 16;
    const int iconSlot = 16;
    const int shortcutWidth = m_shortcutText.isEmpty()
        ? 0
        : metrics.horizontalAdvance(m_shortcutText)
            + ::Spacing::Gap::Normal;
    const int contentWidth =
        ::Spacing::Medium * 2
        + checkSlot
        + ::Spacing::Gap::Tight
        + iconSlot
        + ::Spacing::Gap::Normal
        + metrics.horizontalAdvance(m_displayText)
        + shortcutWidth;
    return QSize(qMax(180, contentWidth),
                 ::Spacing::ControlHeight::Large);
}

QSize CommandPresenter::minimumSizeHint() const
{
    return m_mode == Mode::Primary
        ? QSize(::Spacing::ControlHeight::Large,
                ::Spacing::ControlHeight::Large)
        : QSize(120, ::Spacing::ControlHeight::Large);
}

void CommandPresenter::onThemeUpdated()
{
    setFont(themeFont(Typography::FontRole::Body).toQFont());
    basicinput::Button::onThemeUpdated();
    updateGeometry();
}

void CommandPresenter::paintEvent(QPaintEvent* event)
{
    basicinput::Button::paintEvent(event);
    if (m_mode == Mode::Overflow)
        paintOverflowContent();
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
    const QColor textColor =
        isEnabled() ? colors.textPrimary : colors.textDisabled;
    const QColor shortcutColor =
        isEnabled() ? colors.textSecondary : colors.textDisabled;

    const QRect bounds = rect();
    const QRect content =
        bounds.adjusted(
            ::Spacing::Medium, 0, -::Spacing::Medium, 0);
    int logicalLeading = content.left();
    int logicalTrailing = content.right() + 1;

    const QRect checkLogical(
        logicalLeading,
        content.center().y() - 8,
        16,
        16);
    logicalLeading += 16 + ::Spacing::Gap::Tight;

    const QRect iconLogical(
        logicalLeading,
        content.center().y() - 8,
        16,
        16);
    logicalLeading += 16 + ::Spacing::Gap::Normal;

    const int shortcutWidth = m_shortcutText.isEmpty()
        ? 0
        : painter.fontMetrics().horizontalAdvance(m_shortcutText);
    QRect shortcutLogical;
    if (shortcutWidth > 0) {
        shortcutLogical = QRect(
            logicalTrailing - shortcutWidth,
            content.top(),
            shortcutWidth,
            content.height());
        logicalTrailing -= shortcutWidth + ::Spacing::Gap::Normal;
    }

    const QRect textLogical(
        logicalLeading,
        content.top(),
        qMax(0, logicalTrailing - logicalLeading),
        content.height());

    if (command->isCheckable() && command->isChecked()) {
        painter.setPen(textColor);
        Typography::Icons::paintGlyph(
            painter,
            visualRect(layoutDirection(), bounds, checkLogical),
            Typography::Icons::CheckMark,
            12,
            Qt::AlignCenter);
    }

    if (!m_displayIcon.isNull()) {
        const qreal targetDpr = painter.device()
            ? qMax<qreal>(1.0, painter.device()->devicePixelRatioF())
            : qMax<qreal>(1.0, devicePixelRatioF());
        QPixmap pixmap = fluentIconPixmapForLogicalExtent(
            m_displayIcon,
            QSize(16, 16),
            targetDpr,
            window() ? window()->windowHandle() : nullptr);
        if (!pixmap.isNull()) {
            const QRect target =
                visualRect(layoutDirection(), bounds, iconLogical);
            const QSize logicalSize = fluentPixmapLogicalSize(pixmap);
            const QPoint topLeft(
                target.center().x() - logicalSize.width() / 2,
                target.center().y() - logicalSize.height() / 2);
            painter.drawPixmap(topLeft, pixmap);
        }
    }

    const QRect textVisual =
        visualRect(layoutDirection(), bounds, textLogical);
    const QString elided = painter.fontMetrics().elidedText(
        m_displayText,
        Qt::ElideRight,
        textVisual.width());
    painter.setPen(textColor);
    painter.drawText(
        textVisual,
        QStyle::visualAlignment(
            layoutDirection(),
            Qt::AlignLeft | Qt::AlignVCenter)
            | Qt::TextSingleLine,
        elided);

    if (!shortcutLogical.isEmpty()) {
        painter.setPen(shortcutColor);
        painter.drawText(
            visualRect(layoutDirection(), bounds, shortcutLogical),
            Qt::AlignCenter | Qt::TextSingleLine,
            m_shortcutText);
    }
}

CommandMoreButton::CommandMoreButton(QWidget* parent)
    : basicinput::Button(parent)
{
    markCommandAccessibleWidget(
        this, CommandAccessibleRole::MoreButton);
    setFluentStyle(basicinput::Button::Subtle);
    setFluentLayout(basicinput::Button::IconOnly);
    setIconGlyph(Typography::Icons::More, 20);
    setFocusVisual(true);
    setFocusPolicy(Qt::ClickFocus);
    setMinimumSize(
        ::Spacing::ControlHeight::Large,
        ::Spacing::ControlHeight::Large);
    const QString moreCommands =
        QCoreApplication::translate(
            "fluent::menus_toolbars::CommandBar",
            "More commands");
    setToolTip(moreCommands);
    setAccessibleName(moreCommands);
    setProperty("commandText", moreCommands);
    setExpandedState(false, false);
}

void CommandMoreButton::setExpandedState(
    bool expanded,
    bool expandable)
{
    updateCommandExpandedAccessibility(
        this, expanded, expandable);
}

} // namespace fluent::menus_toolbars::detail
