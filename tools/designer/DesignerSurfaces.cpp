#include "DesignerSurfaces.h"

#include <FluentQt/Collections.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/MenusToolbars.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Windowing.h>
#include <QAction>
#include <QApplication>
#include <QHelpEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace fluentqt_designer {
using namespace fluent::dialogs_flyouts;
using namespace fluent::menus_toolbars;
using namespace fluent::status_info;
using fluent::collections::DrawerView;
using fluent::textfields::Label;

SurfaceButton::SurfaceButton(Surface kind, QWidget* parent) : Button(parent), m_kind(kind)
{
    connect(this, &Button::clicked, this, &SurfaceButton::openSurface);
}

SurfaceButton::~SurfaceButton()
{
    // Open overlays can be reparented to the window by the component.
    delete m_surface.data();
}

QWidget* SurfaceButton::surface()
{
    if (m_surface && m_host == window())
        return m_surface;
    delete m_surface.data();
    m_host = window();
    switch (m_kind) {
    case Surface::Popup:
        m_surface = new Popup(this);
        break;
    case Surface::Flyout:
        m_surface = new Flyout(this);
        break;
    case Surface::TeachingTip:
        m_surface = new TeachingTip(this);
        break;
    case Surface::CoachMark:
        m_surface = new CoachMark(this);
        break;
    case Surface::DrawerView:
        m_surface = new DrawerView(this);
        break;
    case Surface::SplashScreen: {
        auto* shell = qobject_cast<fluent::windowing::Window*>(window());
        m_surface = new SplashScreen(shell ? shell->contentHost() : window());
        break;
    }
    case Surface::Toast:
        m_surface = new Toast(this);
        break;
    case Surface::ToolTip:
        m_surface = ToolTip::attach(this, toolTip());
        break;
    case Surface::Menu:
        m_surface = new FluentMenu(text(), this);
        break;
    case Surface::CommandBarFlyout:
        m_surface = new CommandBarFlyout(this);
        break;
    case Surface::CommandPalette:
        m_surface = new CommandPalette(this);
        break;
    }
    configure(m_surface);
    return m_surface;
}

void SurfaceButton::openSurface()
{
    QWidget* widget = surface();
    const QPoint below(0, height());
    switch (m_kind) {
    case Surface::Popup: {
        auto* popup = static_cast<Popup*>(widget);
        popup->setPosition(this, below);
        popup->open();
        break;
    }
    case Surface::Flyout:
        static_cast<Flyout*>(widget)->showAt(this);
        break;
    case Surface::TeachingTip:
        static_cast<TeachingTip*>(widget)->showAt(this);
        break;
    case Surface::CoachMark: {
        auto* mark = static_cast<CoachMark*>(widget);
        mark->setTarget(this);
        mark->open();
        break;
    }
    case Surface::DrawerView:
        static_cast<DrawerView*>(widget)->open();
        break;
    case Surface::SplashScreen: {
        auto* splash = static_cast<SplashScreen*>(widget);
        splash->show();
        splash->raise();
        if (m_dismissTimer && m_dismissTimer->interval() > 0)
            m_dismissTimer->start();
        break;
    }
    case Surface::Toast:
        static_cast<Toast*>(widget)->present(this);
        break;
    case Surface::ToolTip: {
        QHelpEvent help(QEvent::ToolTip, rect().center(), mapToGlobal(rect().center()));
        QApplication::sendEvent(this, &help);
        break;
    }
    case Surface::Menu:
        static_cast<FluentMenu*>(widget)->popup(mapToGlobal(below));
        break;
    case Surface::CommandBarFlyout:
        static_cast<CommandBarFlyout*>(widget)->showAt(this);
        break;
    case Surface::CommandPalette:
        static_cast<CommandPalette*>(widget)->open();
        break;
    }
}

MessageButton::MessageButton(Surface kind, QWidget* parent) : SurfaceButton(kind, parent), m_text(tr("Message"))
{
    if (kind == Surface::ToolTip)
        surface();
}

void MessageButton::setSurfaceText(const QString& text)
{
    if (m_text != text) {
        m_text = text;
        emit surfaceTextChanged(text);
    }
}

void MessageButton::configure(QWidget* widget)
{
    if (auto* toast = qobject_cast<Toast*>(widget)) {
        toast->setMessage(m_text);
        connect(this, &MessageButton::surfaceTextChanged, toast, &Toast::setMessage);
        return;
    }
    if (auto* tip = qobject_cast<ToolTip*>(widget)) {
        tip->setText(m_text);
        connect(this, &MessageButton::surfaceTextChanged, tip, &ToolTip::setText);
        return;
    }
    if (auto* splash = qobject_cast<SplashScreen*>(widget)) {
        splash->setTitle(m_text);
        connect(this, &MessageButton::surfaceTextChanged, splash, &SplashScreen::setTitle);
        return;
    }
    QWidget* host = widget;
    if (auto* tip = qobject_cast<TeachingTip*>(widget))
        host = tip->contentHost();
    if (auto* mark = qobject_cast<CoachMark*>(widget))
        host = mark->contentHost();
    auto* label = new Label(m_text, host);
    label->setWordWrap(true);
    connect(this, &MessageButton::surfaceTextChanged, label, &Label::setText);
    if (auto* drawer = qobject_cast<DrawerView*>(widget)) {
        drawer->setContentWidget(label, fluent::WidgetOwnership::Owned);
    } else {
        auto* layout = new QVBoxLayout(host);
        layout->addWidget(label);
        if (host == widget)
            widget->resize(240, 96);
    }
}

void TimedMessageButton::setDuration(int milliseconds)
{
    milliseconds = qMax(0, milliseconds);
    if (m_duration != milliseconds) {
        m_duration = milliseconds;
        emit durationChanged(milliseconds);
    }
}

void TimedMessageButton::configure(QWidget* widget)
{
    MessageButton::configure(widget);
    if (auto* toast = qobject_cast<Toast*>(widget)) {
        toast->setDuration(m_duration);
        connect(this, &TimedMessageButton::durationChanged, toast, &Toast::setDuration);
        return;
    }
    auto* splash = static_cast<SplashScreen*>(widget);
    auto* timer = new QTimer(splash);
    timer->setSingleShot(true);
    timer->setInterval(m_duration);
    connect(this, &TimedMessageButton::durationChanged, timer, [timer, splash](int duration) {
        timer->setInterval(duration);
        if (duration > 0 && splash->isVisible())
            timer->start();
        else
            timer->stop();
    });
    connect(timer, &QTimer::timeout, splash, &SplashScreen::dismiss);
    m_dismissTimer = timer;
}

CommandsButton::CommandsButton(Surface kind, QWidget* parent) : SurfaceButton(kind, parent), m_texts{tr("Action")} {}

void CommandsButton::setActionTexts(const QStringList& texts)
{
    if (m_texts == texts)
        return;
    m_texts = texts;
    if (m_menu)
        configure(m_menu);
}

void CommandsButton::configure(QWidget* widget)
{
    for (const auto& action : m_actions) {
        if (!action)
            continue;
        if (auto* palette = qobject_cast<CommandPalette*>(m_menu.data()))
            palette->removeCommand(action);
        else if (auto* flyout = qobject_cast<CommandBarFlyout*>(m_menu.data()))
            flyout->removeAction(action);
        else if (m_menu)
            m_menu->removeAction(action);
        action->deleteLater();
    }
    m_actions.clear();
    m_menu = widget;
    for (const QString& text : m_texts) {
        auto* action = new QAction(text, widget);
        connect(action, &QAction::triggered, this, [this, text] { emit actionTriggered(text); });
        if (auto* palette = qobject_cast<CommandPalette*>(widget))
            palette->addCommand(action);
        else if (auto* flyout = qobject_cast<CommandBarFlyout*>(widget))
            flyout->addAction(action);
        else
            widget->addAction(action);
        m_actions.append(action);
    }
}
} // namespace fluentqt_designer
